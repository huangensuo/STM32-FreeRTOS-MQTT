/*
 * STM32智能室内环境质量监测与安全预警系统
 * 主控芯片：STM32F103C8T6
 * 开发框架：标准外设库 + FreeRTOS
 *
 * FreeRTOS多任务架构：
 *   TaskSensor   (优先级4) — 传感器数据采集，写入 g_SensorData
 *   TaskControl  (优先级3) — 状态机判断，控制执行器
 *   TaskDisplay  (优先级2) — OLED显示
 *   TaskMQTT     (优先级1) — MQTT通信与远程控制
 */

#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "../System/Delay.h"
#include "../Hardware/Storage.h"
#include "AppConfig.h"

/* ── 各任务入口函数声明 ── */
void TaskSensor(void *pvParameters);
void TaskControl(void *pvParameters);
void TaskDisplay(void *pvParameters);
void TaskMQTT(void *pvParameters);
void TaskKey(void *pvParameters);

int main(void)
{
    BaseType_t xReturn;

    /* ── 配置NVIC优先级分组为4位抢占优先级 ── */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);

    /* ── 初始化DWT cycle counter（供Delay_us使用） ── */
    DWT_Init();

    /* ── 创建互斥信号量 ── */
    xSemaphoreSensor = xSemaphoreCreateMutex();
    xSemaphoreStatus = xSemaphoreCreateMutex();
    xSemaphoreOLED   = xSemaphoreCreateMutex();

    if(xSemaphoreSensor == NULL || xSemaphoreStatus == NULL || xSemaphoreOLED == NULL)
    {
        while(1);  /* 信号量创建失败，停机 */
    }

    /* ── 初始化全局数据 ── */
    g_SensorData.Humidity      = 0;
    g_SensorData.Temperature   = 0;
    g_SensorData.SmokeValue    = 0;
    g_SensorData.SmokeFlag     = 0;
    g_SensorData.SmokeReady    = 0;
    g_SensorData.SmokeBaseline = 0;
    g_SensorData.FlameValue    = 0;
    g_SensorData.FlameFlag     = 0;
    g_SensorData.PirFlag       = 0;
    g_DeviceStatus.State        = STATE_NORMAL;
    g_DeviceStatus.MQTTConnected = 0;
    g_DeviceStatus.AiResultNew  = 0;
    g_DeviceStatus.ManualOverrideMask = 0;
    g_DeviceStatus.ManualOverrideMs   = 0;
    g_DeviceStatus.ManualRelay        = 0;
    g_DeviceStatus.ManualBuzzer       = 0;
    g_DeviceStatus.ManualLed          = 0;
    g_DeviceStatus.ManualRgb          = MANUAL_RGB_OFF;

    /* ── 恢复内部Flash历史数据（掉电保存） ── */
    Storage_Init();

    /* ── 恢复内部Flash阈值配置（按键设置，掉电保存） ── */
    Storage_ConfigLoad();

    /* ── 创建FreeRTOS任务 ── */
    xReturn = xTaskCreate(TaskSensor,  "Sensor",  STACK_SENSOR,  NULL, PRIORITY_SENSOR,  NULL);
    if(xReturn != pdPASS) while(1);

    xReturn = xTaskCreate(TaskControl, "Control", STACK_CONTROL, NULL, PRIORITY_CONTROL, NULL);
    if(xReturn != pdPASS) while(1);

    xReturn = xTaskCreate(TaskDisplay, "Display", STACK_DISPLAY, NULL, PRIORITY_DISPLAY, NULL);
    if(xReturn != pdPASS) while(1);

    xReturn = xTaskCreate(TaskKey,     "Key",     STACK_KEY,     NULL, PRIORITY_KEY,     NULL);
    if(xReturn != pdPASS) while(1);

    xReturn = xTaskCreate(TaskMQTT,    "MQTT",    STACK_MQTT,    NULL, PRIORITY_MQTT,    NULL);
    if(xReturn != pdPASS) while(1);

    /* ── 启动FreeRTOS调度器 ── */
    vTaskStartScheduler();

    /* 程序应该不会到达这里 */
    while(1);
}
