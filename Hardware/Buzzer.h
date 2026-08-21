/*
 * 有源蜂鸣器驱动
 * 引脚：PA1（推挽输出）
 * 功能：预警时间歇鸣叫，危险时持续长鸣
 * 硬件特性：低电平触发（GND触发蜂鸣器响）
 */
#ifndef __BUZZER_H
#define __BUZZER_H

#include "stm32f10x.h"

#define BUZZER_RCC    RCC_APB2Periph_GPIOA  // 蜂鸣器时钟使能
#define BUZZER_GPIO   GPIOA                 // 蜂鸣器端口
#define BUZZER_PIN    GPIO_Pin_1            // 蜂鸣器引脚

void Buzzer_Init(void);  // 初始化蜂鸣器
void Buzzer_On(void);    // 开启蜂鸣器（低电平触发）
void Buzzer_Off(void);   // 关闭蜂鸣器（高电平停止）

#endif
