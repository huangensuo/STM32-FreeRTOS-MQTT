/**
 * @file    USART.c
 * @brief   USART1驱动实现
 * @details 提供USART1的初始化、发送、接收功能，支持AT指令通信和MQTT数据透传
 *          硬件连接：PA9(TX) → ESP8266 RX, PA10(RX) → ESP8266 TX
 * @author  派大星
 * @date    2026-06
 */

#include "USART.h"
#include "../System/Delay.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

/* USART1接收缓冲区，存储AT指令响应数据 */
uint8_t USART1_RxBuffer[USART1_RX_BUF_SIZE];
/* 当前接收数据长度 */
volatile uint16_t USART1_RxLen = 0;
/* 已读取的数据位置 */
uint16_t USART1_RxRead = 0;
/* 接收完成标志，接收到换行符'\n'时置1 */
volatile uint8_t USART1_RxFlag = 0;

/* +IPD数据专用缓冲区，用于存储ESP8266透传的MQTT数据包 */
/* 独立于AT指令缓冲区，避免AT解析时被意外清空 */
uint8_t IPD_Buffer[IPD_BUF_SIZE];
/* +IPD数据长度 */
uint16_t IPD_Len = 0;
/* +IPD数据就绪标志，ISR中解析完成后置1 */
volatile uint8_t IPD_Ready = 0;
/* +IPD数据接收计数，用于OLED显示调试 */
volatile uint8_t USART1_SaveIPDCount = 0;
/* 诊断：最近一次超时时的RxBuffer快照 */
char USART1_DiagSnap[DIAG_SNAP_LEN] = {0};

/* 透传模式标志，1=透传模式（直接发送原始MQTT数据包），0=AT指令模式 */
uint8_t USART1_TransparentMode = 0;

/**
 * @brief  USART1初始化函数
 * @param  BaudRate: 波特率（如115200）
 * @note   PA9(TX) → ESP8266 RX, PA10(RX) → ESP8266 TX
 * @retval None
 */
void USART1_Init(uint32_t BaudRate)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    USART_InitTypeDef USART_InitStruct;
    NVIC_InitTypeDef NVIC_InitStruct;

    /* 使能USART1和GPIOA时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);

    /* PA9配置为复用推挽输出（USART1_TX） */
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* PA10配置为上拉输入（USART1_RX） */
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_10;
    GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* 配置USART1参数 */
    USART_InitStruct.USART_BaudRate = BaudRate;
    USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStruct.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_InitStruct.USART_Parity = USART_Parity_No;
    USART_InitStruct.USART_StopBits = USART_StopBits_1;
    USART_InitStruct.USART_WordLength = USART_WordLength_8b;
    USART_Init(USART1, &USART_InitStruct);

    /* 配置USART1中断优先级（4 < configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY=5，确保不被FreeRTOS临界区屏蔽） */
    NVIC_InitStruct.NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 4;
    NVIC_InitStruct.NVIC_IRQChannelSubPriority = 0;
    NVIC_Init(&NVIC_InitStruct);

    /* 使能USART1接收中断 */
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
    /* 使能USART1 */
    USART_Cmd(USART1, ENABLE);
}

/**
 * @brief  发送单个字节
 * @param  Byte: 要发送的字节
 * @note   阻塞式发送，等待发送完成
 * @retval None
 */
void USART1_SendByte(uint8_t Byte)
{
    USART_SendData(USART1, Byte);
    /* 等待发送数据寄存器为空 */
    while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
}

/**
 * @brief  发送字符串（以'\0'结尾）
 * @param  String: 字符串指针
 * @retval None
 */
void USART1_SendString(char *String)
{
    while(*String) USART1_SendByte(*String++);
}

/**
 * @brief  发送指定长度的数据
 * @param  Data: 数据指针
 * @param  Length: 数据长度
 * @retval None
 */
void USART1_SendData(uint8_t *Data, uint16_t Length)
{
    uint16_t i;
    for(i = 0; i < Length; i++) USART1_SendByte(Data[i]);
}

/**
 * @brief  等待指定响应字符串（非阻塞版本，适用于FreeRTOS任务）
 * @param  Response: 期望的响应字符串（如"OK"）
 * @param  Timeout_ms: 超时时间（毫秒）
 * @note   此函数使用vTaskDelay让出CPU，不会阻塞调度器
 *         +IPD数据由ISR独立捕获，不受此函数影响
 * @retval 1: 接收到响应，0: 超时未收到
 */
uint8_t USART1_WaitForResponse(char *Response, uint32_t Timeout_ms)
{
    uint32_t Timer = 0;
    char *p;

    /* 清空接收标志和缓冲区长度，准备接收新数据 */
    USART1_RxFlag = 0;
    USART1_RxLen = 0;

    /* 超时循环等待响应 */
    while(Timer < Timeout_ms)
    {
        /* 检查是否收到完整响应（ISR中每个字节都会置1标志） */
        if(USART1_RxFlag == 1)
        {
            p = (char*)USART1_RxBuffer;
            /* 保存当前缓冲区状态供OLED诊断显示 */
            {
                uint16_t i;
                uint16_t copyLen = (USART1_RxLen < DIAG_SNAP_LEN - 1) ? USART1_RxLen : (DIAG_SNAP_LEN - 1);
                for(i = 0; i < copyLen; i++)
                    USART1_DiagSnap[i] = (char)USART1_RxBuffer[i];
                USART1_DiagSnap[copyLen] = '\0';
            }
            /* 在接收缓冲区中查找期望的响应字符串 */
            if(strstr(p, Response) != NULL)
                return 1;
            /* 未找到，清除标志继续等待 */
            USART1_RxFlag = 0;
        }
        vTaskDelay(1);  /* 使用FreeRTOS延时，让出CPU */
        Timer++;
    }
    /* 超时：确保快照是最新的 */
    {
        uint16_t i;
        uint16_t copyLen = (USART1_RxLen < DIAG_SNAP_LEN - 1) ? USART1_RxLen : (DIAG_SNAP_LEN - 1);
        for(i = 0; i < copyLen; i++)
            USART1_DiagSnap[i] = (char)USART1_RxBuffer[i];
        USART1_DiagSnap[copyLen] = '\0';
    }
    return 0;
}

/**
 * @brief  清空接收缓冲区
 * @retval None
 */
void USART1_ClearRxBuffer(void)
{
    USART1_RxLen = 0;
    USART1_RxRead = 0;
    USART1_RxFlag = 0;
}

/**
 * @brief  压缩接收缓冲区（移动未读数据到缓冲区头部）
 * @note   当已读数据超过缓冲区1/3时触发压缩，避免缓冲区溢出
 * @retval None
 */
void USART1_CompactRxBuffer(void)
{
    /* 如果没有未读数据，直接重置缓冲区 */
    if(USART1_RxRead >= USART1_RxLen)
    {
        USART1_RxLen = 0;
        USART1_RxRead = 0;
    }
    /* 如果已读数据超过缓冲区1/3，移动未读数据到头部 */
    else if(USART1_RxRead > USART1_RX_BUF_SIZE / 3)
    {
        uint16_t unread = USART1_RxLen - USART1_RxRead;
        uint16_t i;
        for(i = 0; i < unread; i++)
            USART1_RxBuffer[i] = USART1_RxBuffer[USART1_RxRead + i];
        USART1_RxLen = unread;
        USART1_RxRead = 0;
    }
}

/**
 * @brief  保存+IPD数据（占位函数，实际处理在ISR中）
 * @note   此函数由MQTT_ProcessRx在主循环中调用
 * @retval None
 */
void USART1_SaveIPD(void)
{
	/* Called from MQTT_ProcessRx in main loop */
}

/**
 * @brief  检查+IPD数据（占位函数，实际处理在ISR中）
 * @note   +IPD数据由USART1 ISR在stm32f10x_it.c中解析和存储
 * @retval None
 */
void USART1_CheckIPD(void)
{
	/* IPD capture handled by USART1 ISR in stm32f10x_it.c */
}

/**
 * @brief  透传模式网络读取函数（用于paho.mqtt.embedded-c）
 * @param  buffer: 接收缓冲区
 * @param  len: 期望读取长度
 * @param  timeout_ms: 超时时间（毫秒）
 * @retval 实际读取的字节数，0表示超时
 */
int USART1_NetworkRead(unsigned char* buffer, int len, unsigned long timeout_ms)
{
    uint32_t timer = 0;
    int read = 0;
    
    while(read < len && timer < timeout_ms)
    {
        /* 检查是否有新数据（IPD数据就绪） */
        if(IPD_Ready == 1)
        {
            /* 复制IPD数据到输出缓冲区 */
            int copy_len = (IPD_Len < len - read) ? IPD_Len : (len - read);
            memcpy(buffer + read, IPD_Buffer, copy_len);
            read += copy_len;
            
            /* 清除IPD就绪标志 */
            IPD_Ready = 0;
            USART1_SaveIPDCount++;
        }
        
        /* 检查USART1接收缓冲区（用于非IPD数据） */
        if(USART1_RxLen > 0)
        {
            buffer[read++] = USART1_RxBuffer[USART1_RxRead++];
            if(USART1_RxRead >= USART1_RxLen)
            {
                USART1_RxLen = 0;
                USART1_RxRead = 0;
            }
        }
        
        vTaskDelay(1);
        timer++;
    }
    
    return read;
}

/**
 * @brief  透传模式网络写入函数（用于paho.mqtt.embedded-c）
 * @param  buffer: 发送数据
 * @param  len: 数据长度
 * @param  timeout_ms: 超时时间（毫秒，忽略）
 * @retval 实际发送的字节数
 */
int USART1_NetworkWrite(unsigned char* buffer, int len, unsigned long timeout_ms)
{
    /* 透传模式下直接发送原始数据 */
    USART1_SendData(buffer, len);
    return len;
}

/**
 * @brief  设置透传模式
 * @param  mode: 1=透传模式，0=AT指令模式
 * @retval None
 */
void USART1_SetTransparentMode(uint8_t mode)
{
    USART1_TransparentMode = mode;
}
