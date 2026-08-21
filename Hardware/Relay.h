/*
 * 继电器驱动（控制散热风扇）
 * 引脚：PA11（推挽输出）
 * 功能：温湿度超标预警或危险状态时开启，控制散热风扇
 * 硬件特性：低电平吸合，高电平释放
 */
#ifndef __RELAY_H
#define __RELAY_H

#include "stm32f10x.h"

#define RELAY_RCC    RCC_APB2Periph_GPIOA  // 继电器时钟使能
#define RELAY_GPIO   GPIOA                 // 继电器端口
#define RELAY_PIN    GPIO_Pin_11           // 继电器引脚

void Relay_Init(void);   // 初始化继电器
void Relay_Open(void);   // 开启继电器（吸合，接通风扇）
void Relay_Close(void);  // 关闭继电器（释放，断开风扇）

#endif
