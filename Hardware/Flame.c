/*
 * 火焰传感器驱动实现（ADC模拟量采集）
 * 引脚：PA3（ADC1_CH3）
 * 功能：检测明火，使用ADC1单通道单次转换模式
 * 硬件原理：火焰传感器对红外线敏感，有火焰时接收管电阻降低，ADC值减小
 */
#include "Flame.h"

/**
 * @brief 初始化火焰传感器ADC
 * @note 配置PA3为模拟输入，ADC1为独立模式、单次转换、软件触发
 */
void Flame_ADC_Init(void)
{
    GPIO_InitTypeDef        GPIO_InitStruct;
    ADC_InitTypeDef         ADC_InitStruct;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_ADC1, ENABLE);  // 使能GPIOA和ADC1时钟

    GPIO_InitStruct.GPIO_Pin   = FLAME_GPIO_PIN;  // 配置PA3
    GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_AIN;   // 模拟输入模式
    GPIO_Init(FLAME_GPIO_PORT, &GPIO_InitStruct); // 初始化GPIO

    // ADC配置
    ADC_InitStruct.ADC_Mode               = ADC_Mode_Independent;  // 独立模式
    ADC_InitStruct.ADC_ScanConvMode       = DISABLE;               // 禁止扫描
    ADC_InitStruct.ADC_ContinuousConvMode = DISABLE;               // 单次转换模式
    ADC_InitStruct.ADC_ExternalTrigConv   = ADC_ExternalTrigConv_None;  // 软件触发
    ADC_InitStruct.ADC_DataAlign          = ADC_DataAlign_Right;   // 右对齐
    ADC_InitStruct.ADC_NbrOfChannel       = 1;                    // 单通道
    ADC_Init(ADC1, &ADC_InitStruct);                              // 初始化ADC1

    ADC_Cmd(ADC1, ENABLE);                   // 使能ADC1
    ADC_ResetCalibration(ADC1);              // 重置校准寄存器
    while(ADC_GetResetCalibrationStatus(ADC1));  // 等待重置完成
    ADC_StartCalibration(ADC1);              // 开始校准
    while(ADC_GetCalibrationStatus(ADC1));   // 等待校准完成
}

/**
 * @brief 获取火焰传感器ADC值
 * @return ADC转换结果（0~4095）
 * @note ADC值越低表示火焰越强烈，值越小红外强度越大
 */
uint16_t Flame_Get_AdcValue(void)
{
    ADC_RegularChannelConfig(ADC1, FLAME_ADC_CHANNEL, 1, ADC_SampleTime_28Cycles5);  // 配置规则通道
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);  // 软件触发转换
    while(!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC));  // 等待转换完成
    return ADC_GetConversionValue(ADC1);      // 返回转换结果
}
