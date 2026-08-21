#include "AppConfig.h"

/* ── 全局数据共享区 ── */
SensorDataStruct    g_SensorData;
DeviceStatusStruct  g_DeviceStatus;
ThresholdConfigTypeDef g_Threshold = {
    TEMP_WARN_DEFAULT,
    HUM_WARN_DEFAULT,
    SMOKE_WARN_OFFSET_DEFAULT,
    SMOKE_ALARM_OFFSET_DEFAULT,
    SMOKE_EMERGENCY_OFFSET_DEFAULT
};
volatile uint8_t    g_KeyShort = 0;
volatile uint8_t    g_KeyLong = 0;
volatile uint8_t    g_DisplayPage = 0;
volatile uint8_t    g_BuzzerMute = 0;
volatile uint8_t    g_FanManual = 0;

/* ── FreeRTOS信号量句柄 ── */
SemaphoreHandle_t   xSemaphoreSensor = NULL;
SemaphoreHandle_t   xSemaphoreStatus = NULL;
SemaphoreHandle_t   xSemaphoreOLED = NULL;
