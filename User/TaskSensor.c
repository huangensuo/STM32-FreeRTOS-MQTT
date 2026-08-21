#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "../Hardware/DHT11.h"
#include "../Hardware/MQ2.h"
#include "../Hardware/Flame.h"
#include "../Hardware/PIR.h"
#include "AppConfig.h"

/* ── DHT11定时 ── */
static uint16_t s_DHT_Timer  = 0;

/* ── 火焰/烟雾检测去抖计数器（防止ADC抖动导致状态机快速震荡） ── */
#define DEBOUNCE_ON   3   /* 连续3次满足条件才置位 */
static uint8_t  s_FlameDebounce = 0;
static uint8_t  s_SmokeDebounce = 0;

/* ── MQ2上电预热计时（预热期间只显示数值，不判定烟雾） ── */
static uint32_t s_SmokeWarmup = 0;

/* ── MQ2自动校准：预热期间累计ADC求基准线 ── */
static uint32_t s_SmokeCalSum    = 0;   /* 预热期间ADC累加值 */
static uint32_t s_SmokeCalCount  = 0;   /* 预热期间采样次数 */
static uint16_t s_SmokeBaseline  = 0;   /* 校准后的洁净空气基准ADC值 */
static uint8_t  s_SmokeCalDone   = 0;   /* 基准线已计算标志 */

/* ── 传感器采集任务 ── */
void TaskSensor(void *pvParameters)
{
    TickType_t xLastWakeTime;

    Flame_ADC_Init();
    MQ2_ADC_Init();
    PIR_Init();

    xLastWakeTime = xTaskGetTickCount();

    while(1)
    {
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(10));

        /* ── MQ2预热计时 ── */
        s_SmokeWarmup += 10;

        /* ── 人体检测（HC-SR312，标志由EXTI中断根据引脚电平更新） ── */
        if(xSemaphoreTake(xSemaphoreSensor, portMAX_DELAY) == pdTRUE)
        {
            g_SensorData.PirFlag = PIR_Flag;
            xSemaphoreGive(xSemaphoreSensor);
        }

        /* ── 烟雾检测（每次10ms采样，含自动校准） ── */
        {
            uint16_t smokeValue = MQ2_Get_AdcValue();

            /* 预热期间：累计ADC求基准线 */
            if(s_SmokeWarmup < SMOKE_WARMUP_MS)
            {
                s_SmokeCalSum += smokeValue;
                s_SmokeCalCount++;
            }

            /* 预热刚结束：计算洁净空气基准线（只算一次） */
            if(s_SmokeWarmup >= SMOKE_WARMUP_MS && s_SmokeCalDone == 0)
            {
                s_SmokeCalDone = 1;
                if(s_SmokeCalCount > 0)
                    s_SmokeBaseline = (uint16_t)(s_SmokeCalSum / s_SmokeCalCount);
            }

            /* 相对基准线的差值（防下溢：仅当读数高于基准线才计算） */
            {
                uint16_t smokeDelta = 0;
                if(s_SmokeBaseline > 0 && smokeValue > s_SmokeBaseline)
                    smokeDelta = smokeValue - s_SmokeBaseline;

                if(xSemaphoreTake(xSemaphoreSensor, portMAX_DELAY) == pdTRUE)
                {
                    g_SensorData.SmokeValue = smokeValue;
                    g_SensorData.SmokeBaseline = s_SmokeBaseline;

                    /* 预热完成前不判定烟雾（数值照常显示，方便观察预热过程） */
                    if(s_SmokeWarmup >= SMOKE_WARMUP_MS)
                    {
                        uint16_t smokeAlarm = g_Threshold.SmokeAlarmOffset;   /* 运行时阈值（按键可调） */

                        g_SensorData.SmokeReady = 1;
                        /* 去抖处理：差值连续多次达到报警偏移才置位，连续多次低于才清除 */
                        if(smokeDelta >= smokeAlarm)
                        {
                            if(s_SmokeDebounce < DEBOUNCE_ON)
                                s_SmokeDebounce++;
                            if(s_SmokeDebounce >= DEBOUNCE_ON)
                            {
                                g_SensorData.SmokeFlag = 1;
                            }
                        }
                        else
                        {
                            if(s_SmokeDebounce > 0)
                                s_SmokeDebounce--;
                            else
                                g_SensorData.SmokeFlag = 0;
                        }
                    }
                    else
                    {
                        s_SmokeDebounce = 0;
                        g_SensorData.SmokeFlag = 0;
                    }
                    xSemaphoreGive(xSemaphoreSensor);
                }
            }
        }

        /* ── 火焰检测（每次10ms采样） ── */
        {
            uint16_t flameValue = Flame_Get_AdcValue();
            if(xSemaphoreTake(xSemaphoreSensor, portMAX_DELAY) == pdTRUE)
            {
                g_SensorData.FlameValue = flameValue;
                /* 去抖处理：连续多次低于阈值才置位，连续多次高于阈值才清除 */
                if(flameValue < FLAME_THRESHOLD)
                {
                    if(s_FlameDebounce < DEBOUNCE_ON)
                        s_FlameDebounce++;
                    if(s_FlameDebounce >= DEBOUNCE_ON)
                    {
                        g_SensorData.FlameFlag = 1;
                    }
                }
                else
                {
                    if(s_FlameDebounce > 0)
                        s_FlameDebounce--;
                    else
                        g_SensorData.FlameFlag = 0;
                }
                xSemaphoreGive(xSemaphoreSensor);
            }
        }

        /* ── DHT11：每1000ms读取一次 ── */
        s_DHT_Timer += 10;
        if(s_DHT_Timer >= 1000)
        {
            s_DHT_Timer = 0;
            uint8_t humidity = 0, temperature = 0;
            /* 先释放信号量再读取DHT11（避免DHT11的20ms阻塞持有信号量） */
            DHT11_ReadData(&humidity, &temperature);
            if(xSemaphoreTake(xSemaphoreSensor, portMAX_DELAY) == pdTRUE)
            {
                g_SensorData.Humidity = humidity;
                g_SensorData.Temperature = temperature;
                xSemaphoreGive(xSemaphoreSensor);
            }
        }

    }
}
