/*
 * RGB三色状态指示灯驱动
 * 引脚：PB12(红)、PB13(绿)、PB14(蓝)
 * 功能：根据设备状态显示不同颜色
 *   - 正常：绿色常亮
 *   - 预警：蓝色慢闪
 *   - 危险：红色快闪
 */
#ifndef __RGB_H_
#define __RGB_H_

#include "stm32f10x.h"

#define RGB_R_PIN   GPIO_Pin_12  // 红色LED引脚
#define RGB_G_PIN   GPIO_Pin_13  // 绿色LED引脚
#define RGB_B_PIN   GPIO_Pin_14  // 蓝色LED引脚
#define RGB_GPIO    GPIOB        // RGB灯端口

void RGB_Init(void);    // 初始化RGB灯

void RGB_Off(void);     // 关闭所有LED
void RGB_Red(void);     // 显示红色
void RGB_Green(void);   // 显示绿色
void RGB_Blue(void);    // 显示蓝色
void RGB_Yellow(void);  // 显示黄色（红+绿）

#endif
