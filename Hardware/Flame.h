/*
 * 火焰传感器驱动（ADC模拟量采集）
 * 引脚：PA3（ADC1_CH3）
 * 功能：检测明火，ADC值越低表示火焰越强烈
 * 硬件特性：
 *   - 无火焰时：ADC值较高（约800~4095）
 *   - 有火焰时：ADC值降低（约0~800）
 *   - 阈值：FLAME_THRESHOLD = 800，ADC值小于800判定为检测到火焰
 */
#ifndef __FLAME_H
#define __FLAME_H

#include "stm32f10x.h"

#define FLAME_GPIO_PORT     GPIOA           // 火焰传感器端口
#define FLAME_GPIO_PIN      GPIO_Pin_3      // 火焰传感器引脚
#define FLAME_ADC_CHANNEL   ADC_Channel_3   // ADC通道

#define FLAME_THRESHOLD     800U            // 火焰检测阈值（ADC值小于此值判定为有火焰）

void Flame_ADC_Init(void);           // 初始化火焰传感器ADC
uint16_t Flame_Get_AdcValue(void);  // 获取火焰传感器ADC值

#endif
