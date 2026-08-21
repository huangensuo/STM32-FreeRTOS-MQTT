/*
 * RGB三色状态指示灯驱动实现
 * 引脚：PB12(红)、PB13(绿)、PB14(蓝)
 * 硬件特性：高电平点亮，低电平熄灭
 * 使用普通GPIO推挽输出，非PWM模式，用于状态指示
 */
#include "stm32f10x.h"
#include "RGB.h"

/**
 * @brief 初始化RGB灯
 * @note 配置PB12/PB13/PB14为推挽输出模式，上电默认关闭
 */
void RGB_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);  // 使能GPIOB时钟

    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;           // 推挽输出
    GPIO_InitStruct.GPIO_Pin = RGB_R_PIN | RGB_G_PIN | RGB_B_PIN;  // 配置三个引脚
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;          // 50MHz速度
    GPIO_Init(RGB_GPIO, &GPIO_InitStruct);                  // 初始化GPIO

    RGB_Off();  // 上电默认关闭所有LED
}

/**
 * @brief 关闭所有RGB LED
 * @note 设置所有引脚为低电平
 */
void RGB_Off(void)
{
    GPIO_ResetBits(RGB_GPIO, RGB_R_PIN | RGB_G_PIN | RGB_B_PIN);
}

/**
 * @brief 显示红色
 * @note 点亮红灯，关闭绿灯和蓝灯
 */
void RGB_Red(void)
{
    GPIO_SetBits(RGB_GPIO, RGB_R_PIN);
    GPIO_ResetBits(RGB_GPIO, RGB_G_PIN | RGB_B_PIN);
}

/**
 * @brief 显示绿色
 * @note 点亮绿灯，关闭红灯和蓝灯
 */
void RGB_Green(void)
{
    GPIO_SetBits(RGB_GPIO, RGB_G_PIN);
    GPIO_ResetBits(RGB_GPIO, RGB_R_PIN | RGB_B_PIN);
}

/**
 * @brief 显示蓝色
 * @note 点亮蓝灯，关闭红灯和绿灯
 */
void RGB_Blue(void)
{
    GPIO_SetBits(RGB_GPIO, RGB_B_PIN);
    GPIO_ResetBits(RGB_GPIO, RGB_R_PIN | RGB_G_PIN);
}

/**
 * @brief 显示黄色
 * @note 同时点亮红灯和绿灯，关闭蓝灯
 */
void RGB_Yellow(void)
{
    GPIO_SetBits(RGB_GPIO, RGB_R_PIN | RGB_G_PIN);
    GPIO_ResetBits(RGB_GPIO, RGB_B_PIN);
}
