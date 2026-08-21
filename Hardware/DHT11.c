/*
 * DHT11温湿度传感器驱动实现（单总线协议）
 * 引脚：PB15（单总线）
 * 协议时序：
 *   1. 主机拉低总线至少18ms，然后释放
 *   2. DHT11拉低总线80us，然后拉高80us作为响应
 *   3. DHT11开始发送40位数据（高位在前）
 *   4. 每一位数据：低电平50us，高电平持续时间决定是0还是1
 *      - 0：高电平约26~28us
 *      - 1：高电平约70us
 * 注意：添加了超时机制，防止传感器未连接时死循环
 */
#include "stm32f10x.h"
#include "DHT11.h"
#include "Delay.h"
#include "FreeRTOS.h"
#include "task.h"

#define DHT11_TIMEOUT 10000  // 超时计数器最大值

/**
 * @brief 设置DHT11引脚为输出模式
 * @note 配置为开漏输出，用于主机发送信号
 */
void DHT11_OUT_Mode(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);  // 使能GPIOB时钟
    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_OD;   // 开漏输出
    GPIO_InitStruct.GPIO_Pin = DHT11_PIN;           // 配置PB15
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;  // 50MHz速度
    GPIO_Init(DHT11_GPIO, &GPIO_InitStruct);        // 初始化GPIO
}

/**
 * @brief 设置DHT11引脚为输入模式
 * @note 配置为上拉输入，用于接收DHT11数据
 */
void DHT11_IN_Mode(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPU;      // 上拉输入
    GPIO_InitStruct.GPIO_Pin = DHT11_PIN;           // 配置PB15
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;  // 50MHz速度
    GPIO_Init(DHT11_GPIO, &GPIO_InitStruct);        // 初始化GPIO
}

/**
 * @brief 从DHT11读取一个字节
 * @return 读取的字节（0x00~0xFF），超时返回0xFF
 * @note 每一位数据由低电平起始位和高电平数据位组成
 */
uint8_t DHT11_ReadByte(void)
{
    uint8_t dat = 0, i;
    uint32_t timeout;

    for(i = 0; i < 8; i++)
    {
        timeout = DHT11_TIMEOUT;
        while(GPIO_ReadInputDataBit(DHT11_GPIO, DHT11_PIN) == 0)  // 等待低电平结束
        {
            if(--timeout == 0) return 0xFF;  // 超时返回错误
        }

        Delay_us(30);  // 延时30us，区分0和1

        if(GPIO_ReadInputDataBit(DHT11_GPIO, DHT11_PIN) == 1)  // 读取数据位
            dat |= (1 << (7 - i));  // 高位在前

        timeout = DHT11_TIMEOUT;
        while(GPIO_ReadInputDataBit(DHT11_GPIO, DHT11_PIN) == 1)  // 等待高电平结束
        {
            if(--timeout == 0) return 0xFF;  // 超时返回错误
        }
    }
    return dat;
}

/**
 * @brief 从DHT11读取温湿度数据
 * @param hum 湿度指针（返回值，0~100%）
 * @param temp 温度指针（返回值，0~50°C）
 * @return 1表示成功，0表示失败
 * @note 数据格式：湿度整数、湿度小数、温度整数、温度小数、校验和
 */
uint8_t DHT11_ReadData(uint8_t *hum, uint8_t *temp)
{
    uint8_t buf[5];
    uint32_t timeout;

    DHT11_OUT_Mode();                     // 设置为输出模式
    GPIO_ResetBits(DHT11_GPIO, DHT11_PIN);  // 拉低总线至少18ms
    vTaskDelay(20);
    GPIO_SetBits(DHT11_GPIO, DHT11_PIN);   // 释放总线
    Delay_us(40);

    DHT11_IN_Mode();                      // 设置为输入模式

    // 等待DHT11响应
    timeout = DHT11_TIMEOUT;
    while(GPIO_ReadInputDataBit(DHT11_GPIO, DHT11_PIN) == 1)  // 等待总线拉低
    {
        if(--timeout == 0) return 0;
    }

    timeout = DHT11_TIMEOUT;
    while(GPIO_ReadInputDataBit(DHT11_GPIO, DHT11_PIN) == 0)  // 等待响应低电平结束
    {
        if(--timeout == 0) return 0;
    }

    timeout = DHT11_TIMEOUT;
    while(GPIO_ReadInputDataBit(DHT11_GPIO, DHT11_PIN) == 1)  // 等待响应高电平结束
    {
        if(--timeout == 0) return 0;
    }

    // 读取40位数据
    buf[0] = DHT11_ReadByte();  // 湿度整数
    buf[1] = DHT11_ReadByte();  // 湿度小数（通常为0）
    buf[2] = DHT11_ReadByte();  // 温度整数
    buf[3] = DHT11_ReadByte();  // 温度小数（通常为0）
    buf[4] = DHT11_ReadByte();  // 校验和

    // 检查超时错误
    if(buf[0] == 0xFF || buf[1] == 0xFF || buf[2] == 0xFF || buf[3] == 0xFF || buf[4] == 0xFF)
        return 0;

    // 校验数据
    if(buf[0] + buf[1] + buf[2] + buf[3] == buf[4])
    {
        *hum = buf[0];   // 返回湿度整数
        *temp = buf[2];  // 返回温度整数
        return 1;        // 成功
    }
    return 0;  // 校验失败
}
