/*
 * 继电器驱动实现（控制散热风扇）
 * 引脚：PA11（推挽输出）
 * 硬件特性：低电平吸合继电器，高电平释放继电器
 * 继电器模块通常需要外接电源（5V），STM32仅提供控制信号
 */
#include "Relay.h"

/**
 * @brief 初始化继电器
 * @note 配置PA11为推挽输出模式，上电默认关闭（高电平释放）
 */
void Relay_Init(void)
{
    RCC_APB2PeriphClockCmd(RELAY_RCC, ENABLE);  // 使能GPIOA时钟

    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;   // 推挽输出
    GPIO_InitStruct.GPIO_Pin  = RELAY_PIN;          // 配置PA11
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;  // 50MHz速度
    GPIO_Init(RELAY_GPIO, &GPIO_InitStruct);        // 初始化GPIO

    Relay_Close();  // 上电默认关闭继电器，防止误触发
}

/**
 * @brief 开启继电器
 * @note 设置PA11为低电平，继电器吸合，接通风扇电源
 */
void Relay_Open(void)
{
    GPIO_ResetBits(RELAY_GPIO, RELAY_PIN);
}

/**
 * @brief 关闭继电器
 * @note 设置PA11为高电平，继电器释放，断开风扇电源
 */
void Relay_Close(void)
{
    GPIO_SetBits(RELAY_GPIO, RELAY_PIN);
}
