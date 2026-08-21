/*
 * 报警指示灯驱动实现
 * 引脚：PA0（推挽输出）
 * 硬件特性：高电平点亮，低电平熄灭
 */
#include "LED.h"

/**
 * @brief 初始化报警指示灯
 * @note 配置PA0为推挽输出模式，上电默认熄灭
 */
void LED_Init(void)
{
    GPIO_InitTypeDef gp;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);  // 使能GPIOA时钟
    gp.GPIO_Pin = LED_PIN;                                  // 配置PA0
    gp.GPIO_Mode = GPIO_Mode_Out_PP;                        // 推挽输出
    gp.GPIO_Speed = GPIO_Speed_50MHz;                       // 50MHz速度
    GPIO_Init(LED_PORT, &gp);                               // 初始化GPIO
    LED_Off();                                              // 上电默认熄灭
}

/**
 * @brief 点亮报警指示灯
 * @note 设置PA0为高电平
 */
void LED_On(void)
{
    GPIO_SetBits(LED_PORT, LED_PIN);
}

/**
 * @brief 熄灭报警指示灯
 * @note 设置PA0为低电平
 */
void LED_Off(void)
{
    GPIO_ResetBits(LED_PORT, LED_PIN);
}
