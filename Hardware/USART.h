#ifndef __USART_H
#define __USART_H

#include "stm32f10x.h"
#include <stdio.h>

/* USART1接收缓冲区大小，用于存储AT指令响应数据 */
#define USART1_RX_BUF_SIZE  512
/* USART1发送缓冲区大小 */
#define USART1_TX_BUF_SIZE  512

/* +IPD数据专用缓冲区大小，用于存储ESP8266透传的MQTT数据包 */
/* 独立于AT指令缓冲区，避免AT解析时被意外清空 */
#define IPD_BUF_SIZE  512

/* USART1接收缓冲区，存储AT指令响应数据 */
extern uint8_t USART1_RxBuffer[USART1_RX_BUF_SIZE];
/* 当前接收数据长度 */
extern volatile uint16_t USART1_RxLen;
/* 已读取的数据位置 */
extern uint16_t USART1_RxRead;
/* 接收完成标志，接收到换行符'\n'时置1（volatile：ISR和任务间共享） */
extern volatile uint8_t USART1_RxFlag;

/* +IPD数据专用缓冲区，用于存储ESP8266透传的MQTT数据包 */
/* 独立于AT指令缓冲区，避免AT解析时被意外清空 */
extern uint8_t IPD_Buffer[IPD_BUF_SIZE];
/* +IPD数据长度 */
extern uint16_t IPD_Len;
/* +IPD数据就绪标志，ISR中解析完成后置1 */
extern volatile uint8_t IPD_Ready;
/* +IPD数据接收计数，用于OLED显示调试 */
extern volatile uint8_t USART1_SaveIPDCount;
/* 诊断：最近一次超时时的RxBuffer快照（前32字节） */
#define DIAG_SNAP_LEN  32
extern char USART1_DiagSnap[DIAG_SNAP_LEN];

/**
 * @brief  USART1初始化函数
 * @param  BaudRate: 波特率（如115200）
 * @note   PA9(TX) → ESP8266 RX, PA10(RX) → ESP8266 TX
 * @retval None
 */
void USART1_Init(uint32_t BaudRate);

/**
 * @brief  发送单个字节
 * @param  Byte: 要发送的字节
 * @retval None
 */
void USART1_SendByte(uint8_t Byte);

/**
 * @brief  发送字符串（以'\0'结尾）
 * @param  String: 字符串指针
 * @retval None
 */
void USART1_SendString(char *String);

/**
 * @brief  发送指定长度的数据
 * @param  Data: 数据指针
 * @param  Length: 数据长度
 * @retval None
 */
void USART1_SendData(uint8_t *Data, uint16_t Length);

/**
 * @brief  等待指定响应字符串
 * @param  Response: 期望的响应字符串（如"OK"）
 * @param  Timeout_ms: 超时时间（毫秒）
 * @note   此函数会清空接收缓冲区，仅用于AT指令响应等待
 *         +IPD数据由ISR独立捕获，不受此函数影响
 * @retval 1: 接收到响应，0: 超时未收到
 */
uint8_t USART1_WaitForResponse(char *Response, uint32_t Timeout_ms);

/**
 * @brief  清空接收缓冲区
 * @retval None
 */
void USART1_ClearRxBuffer(void);

/**
 * @brief  压缩接收缓冲区（移动未读数据到缓冲区头部）
 * @note   当已读数据超过缓冲区1/3时触发压缩，避免缓冲区溢出
 * @retval None
 */
void USART1_CompactRxBuffer(void);

/**
 * @brief  保存+IPD数据（占位函数，实际处理在ISR中）
 * @retval None
 */
void USART1_SaveIPD(void);

/**
 * @brief  检查+IPD数据（占位函数，实际处理在ISR中）
 * @retval None
 */
void USART1_CheckIPD(void);

/**
 * @brief  透传模式网络读取函数（用于paho.mqtt.embedded-c）
 * @param  buffer: 接收缓冲区
 * @param  len: 期望读取长度
 * @param  timeout_ms: 超时时间（毫秒）
 * @retval 实际读取的字节数，0表示超时
 */
int USART1_NetworkRead(unsigned char* buffer, int len, unsigned long timeout_ms);

/**
 * @brief  透传模式网络写入函数（用于paho.mqtt.embedded-c）
 * @param  buffer: 发送数据
 * @param  len: 数据长度
 * @param  timeout_ms: 超时时间（毫秒）
 * @retval 实际发送的字节数
 */
int USART1_NetworkWrite(unsigned char* buffer, int len, unsigned long timeout_ms);

/**
 * @brief  设置透传模式
 * @param  mode: 1=透传模式，0=AT指令模式
 * @retval None
 */
void USART1_SetTransparentMode(uint8_t mode);

#endif
