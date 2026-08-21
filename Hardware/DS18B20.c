/*
 * DS18B20高精度温度传感器驱动实现（单总线协议）- 非阻塞版本
 * 引脚：PA4（单总线）
 * 协议时序：
 *   1. 复位：主机拉低至少480us，释放后等待DS18B20响应
 *   2. 应答：DS18B20拉低60~240us表示存在
 *   3. 写操作：每一位拉低至少60us，在拉低后15us内写入数据
 *   4. 读操作：主机拉低1~15us后释放，在拉低后60us内读取数据
 * 温度转换流程（非阻塞）：
 *   1. DS_StartConvert() - 发送复位和转换命令，立即返回
 *   2. 等待至少750ms
 *   3. DS_ReadTemp() - 读取温度数据
 */
#include "DS18B20.h"
#include "delay.h"
#include "FreeRTOS.h"
#include "task.h"

/* 传感器状态 */
static uint8_t DS_State = 0;
static uint8_t DS_Ready = 0;
static float DS_LastTemp = 0.0f;

/**
 * @brief 初始化DS18B20
 * @note 设置引脚为输出模式，初始电平为高
 */
void DS_Init(void)
{
    DS_OUT();  // 设置为输出模式
    DS_H;      // 初始电平为高
    DS_State = 0;
    DS_Ready = 0;
}

/**
 * @brief 复位DS18B20并检测存在
 * @return 1表示检测到DS18B20，0表示未检测到
 */
uint8_t DS_Rst(void)
{
    uint8_t ack;
    DS_OUT();   // 设置为输出模式
    DS_L;       // 拉低总线
    vTaskDelay(1);  // 保持低电平至少480us
    DS_H;       // 释放总线
    Delay_us(40);
    DS_IN();    // 设置为输入模式
    ack = DS_RD;  // 读取应答信号
    Delay_us(120);
    DS_OUT();   // 设置为输出模式
    DS_H;       // 恢复总线为高电平
    return (ack == 0) ? 1 : 0;  // 低电平表示有应答
}

/**
 * @brief 向DS18B20写入一个字节
 * @param dat 要写入的数据
 * @note 低位在前
 */
void DS_W(uint8_t dat)
{
    uint8_t i;
    DS_OUT();  // 设置为输出模式
    for(i=0; i<8; i++)
    {
        DS_L;       // 拉低总线开始写一位
        Delay_us(10);
        if(dat & 1) DS_H;  // 写入1
        else DS_L;          // 写入0
        Delay_us(70);
        DS_H;       // 释放总线
        Delay_us(20);
        dat >>= 1;  // 处理下一位
    }
}

/**
 * @brief 从DS18B20读取一个字节
 * @return 读取的字节
 * @note 低位在前
 */
uint8_t DS_R(void)
{
    uint8_t i, dat=0;
    for(i=0; i<8; i++)
    {
        DS_OUT();   // 设置为输出模式
        DS_L;       // 拉低总线开始读一位
        Delay_us(2);
        DS_IN();    // 设置为输入模式
        Delay_us(12);
        if(DS_RD) dat |= (1<<i);  // 读取数据位
        Delay_us(50);
        DS_OUT();   // 设置为输出模式
        DS_H;       // 释放总线
        Delay_us(2);
    }
    return dat;
}

/**
 * @brief 启动温度转换（非阻塞）
 * @note 发送转换命令后立即返回，需要等待至少750ms后调用DS_ReadTemp()
 * @return 1表示成功启动，0表示传感器未连接
 */
uint8_t DS_StartConvert(void)
{
    if(!DS_Rst()) return 0;  // 未检测到传感器
    DS_W(0xCC);   // 跳过ROM命令（单传感器时使用）
    DS_W(0x44);   // 温度转换命令
    DS_Ready = 0;
    return 1;
}

/**
 * @brief 读取温度值（非阻塞）
 * @note 必须在DS_StartConvert()调用后至少750ms才能调用
 * @return 温度值（浮点数，单位°C），失败返回-999.0
 */
float DS_ReadTemp(void)
{
    uint8_t L, H;
    int16_t temp;

    if(!DS_Rst()) return -999.0f;  // 未检测到传感器，返回错误

    DS_W(0xCC);   // 跳过ROM命令
    DS_W(0xBE);   // 读取暂存器命令

    L = DS_R();   // 读取低字节
    H = DS_R();   // 读取高字节

    temp = (H << 8) | L;          // 合并高低字节
    DS_LastTemp = temp * 0.0625f; // 转换为浮点数温度
    DS_Ready = 1;
    return DS_LastTemp;
}

/**
 * @brief 获取最后一次读取的温度值
 * @return 温度值（浮点数，单位°C）
 */
float DS_GetLastTemp(void)
{
    return DS_LastTemp;
}

/**
 * @brief 检查温度是否已就绪
 * @return 1表示温度已就绪，0表示未就绪
 */
uint8_t DS_IsReady(void)
{
    return DS_Ready;
}

/* 兼容旧接口的阻塞版本（供初始化使用） */
float DS_GetTemp(void)
{
    uint8_t L, H;
    int16_t temp;

    if(!DS_Rst()) return -999.0f;  // 未检测到传感器，返回错误

    DS_W(0xCC);   // 跳过ROM命令（单传感器时使用）
    DS_W(0x44);   // 温度转换命令
    vTaskDelay(750);  // 等待转换完成（最大750ms）

    DS_Rst();     // 再次复位
    DS_W(0xCC);   // 跳过ROM命令
    DS_W(0xBE);   // 读取暂存器命令

    L = DS_R();   // 读取低字节
    H = DS_R();   // 读取高字节

    temp = (H << 8) | L;          // 合并高低字节
    return temp * 0.0625f;        // 转换为浮点数温度
}
