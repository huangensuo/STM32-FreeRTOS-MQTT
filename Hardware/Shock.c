/*
 * 震动传感器驱动实现（EXTI外部中断）
 * 引脚：PA2（EXTI_Line2）
 * 功能：检测震动/敲击，使用外部中断方式，不占用CPU资源
 * 中断优先级：抢占优先级0，子优先级0（最高优先级）
 */
#include "Shock.h"

volatile uint8_t Shock_Flag = 0;  // 震动触发标志，由中断服务函数置位

/**
 * @brief 初始化震动传感器外部中断
 * @note 配置PA2为上拉输入，EXTI_Line2为双边沿触发中断
 */
void SHOCK_Init(void)
{
    GPIO_InitTypeDef gp;
    EXTI_InitTypeDef exti;
    NVIC_InitTypeDef nvic;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);  // 使能GPIOA和AFIO时钟

    gp.GPIO_Pin = SHOCK_PIN;           // 配置PA2
    gp.GPIO_Mode = GPIO_Mode_IPU;      // 上拉输入模式
    gp.GPIO_Speed = GPIO_Speed_50MHz;  // 50MHz速度
    GPIO_Init(SHOCK_PORT, &gp);        // 初始化GPIO

    GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource2);  // 配置EXTI线

    EXTI_ClearITPendingBit(EXTI_Line2);  // 清除中断标志

    exti.EXTI_Line = EXTI_Line2;                      // EXTI线2
    exti.EXTI_Mode = EXTI_Mode_Interrupt;             // 中断模式
    exti.EXTI_Trigger = EXTI_Trigger_Rising_Falling;  // 双边沿触发
    exti.EXTI_LineCmd = ENABLE;                       // 使能EXTI线
    EXTI_Init(&exti);                                 // 初始化EXTI

    nvic.NVIC_IRQChannel = EXTI2_IRQn;                // EXTI2中断通道
    nvic.NVIC_IRQChannelPreemptionPriority = 5;       // 抢占优先级5（必须 >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY）
    nvic.NVIC_IRQChannelSubPriority = 1;              // 子优先级1
    nvic.NVIC_IRQChannelCmd = ENABLE;                 // 使能中断
    NVIC_Init(&nvic);                                 // 初始化NVIC
}

/**
 * @brief 清除震动触发标志
 * @note 在主程序中处理完震动事件后调用此函数清除标志
 */
void SHOCK_ClearFlag(void)
{
    Shock_Flag = 0;
}


