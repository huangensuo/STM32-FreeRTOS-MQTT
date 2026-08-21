/*
 * 震动传感器驱动（EXTI外部中断）
 * 引脚：PA2（EXTI_Line2）
 * 功能：检测震动/敲击，触发外部中断
 * 硬件特性：
 *   - 震动模块有信号输出时引脚为高电平
 *   - 使用上拉输入，无震动时引脚为低电平
 *   - 中断触发方式：上升沿和下降沿都触发
 */
#ifndef __SHOCK_H
#define __SHOCK_H
#include "stm32f10x.h"

#define SHOCK_PIN    GPIO_Pin_2  // 震动传感器引脚
#define SHOCK_PORT   GPIOA       // 震动传感器端口

extern volatile uint8_t Shock_Flag;  // 震动触发标志（中断服务函数中置位）

void SHOCK_Init(void);        // 初始化震动传感器中断
void SHOCK_ClearFlag(void);   // 清除震动标志

#endif
