/*
 * 有源蜂鸣器驱动实现
 * 引脚：PA1（推挽输出）
 * 硬件特性：低电平触发，高电平停止
 * 注意：有源蜂鸣器内部有振荡器，只需提供高低电平即可控制开关
 */
#include "Buzzer.h"
#include "stm32f10x.h"

/**
 * @brief 初始化蜂鸣器
 * @note 配置PA1为推挽输出模式，上电默认关闭（高电平）
 */
void Buzzer_Init(void)
{
    RCC_APB2PeriphClockCmd(BUZZER_RCC, ENABLE);  // 使能GPIOA时钟

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;   // 推挽输出
    GPIO_InitStructure.GPIO_Pin = BUZZER_PIN;          // 配置PA1
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;  // 50MHz速度
    GPIO_Init(BUZZER_GPIO, &GPIO_InitStructure);       // 初始化GPIO

    Buzzer_Off();  // 上电默认关闭蜂鸣器
}

/**
 * @brief 开启蜂鸣器
 * @note 设置PA1为低电平，触发蜂鸣器鸣叫
 */
void Buzzer_On(void)
{
    GPIO_ResetBits(BUZZER_GPIO, BUZZER_PIN);
}

/**
 * @brief 关闭蜂鸣器
 * @note 设置PA1为高电平，停止蜂鸣器鸣叫
 */
void Buzzer_Off(void)
{
    GPIO_SetBits(BUZZER_GPIO, BUZZER_PIN);
}
