/*
 * DS18B20高精度温度传感器驱动（单总线协议）
 * 引脚：PA4（单总线）
 * 功能：采集高精度温度，分辨率可达12位（0.0625°C）
 * 协议特点：
 *   - 单总线接口，只需一根数据线
 *   - 支持多点组网（多个传感器共用一根总线）
 *   - 温度范围：-55°C ~ +125°C
 *   - 精度：±0.5°C（-10°C ~ +85°C）
 */
#ifndef __DS18B20_H
#define __DS18B20_H
#include "stm32f10x.h"

#define DS_IO    GPIO_Pin_4  // DS18B20单总线引脚
#define DS_PORT  GPIOA       // DS18B20端口

// 快速设置引脚为输出模式（宏定义）
#define DS_OUT() do{GPIO_InitTypeDef gp;RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);gp.GPIO_Pin=DS_IO;gp.GPIO_Mode=GPIO_Mode_Out_PP;gp.GPIO_Speed=GPIO_Speed_50MHz;GPIO_Init(DS_PORT,&gp);}while(0)

// 快速设置引脚为输入模式（宏定义）
#define DS_IN()  do{GPIO_InitTypeDef gp;RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);gp.GPIO_Pin=DS_IO;gp.GPIO_Mode=GPIO_Mode_IPU;GPIO_Init(DS_PORT,&gp);}while(0)

// 快速设置引脚为高电平（宏定义）
#define DS_H     GPIO_SetBits(DS_PORT,DS_IO)

// 快速设置引脚为低电平（宏定义）
#define DS_L     GPIO_ResetBits(DS_PORT,DS_IO)

// 快速读取引脚电平（宏定义）
#define DS_RD    GPIO_ReadInputDataBit(DS_PORT,DS_IO)

void DS_Init(void);      // 初始化DS18B20
uint8_t DS_Rst(void);    // 复位DS18B20并检测存在
void DS_W(uint8_t dat);  // 向DS18B20写入一个字节
uint8_t DS_R(void);      // 从DS18B20读取一个字节
float DS_GetTemp(void);  // 获取温度值（浮点数，阻塞版本）
uint8_t DS_StartConvert(void);  // 启动温度转换（非阻塞）
float DS_ReadTemp(void);  // 读取温度值（非阻塞，需在StartConvert后调用）

#endif
