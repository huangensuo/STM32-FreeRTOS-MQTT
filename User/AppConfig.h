#ifndef __APPCONFIG_H
#define __APPCONFIG_H

#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"

/* ── 温湿度预警阈值默认值（运行时变量，按键可调，保存到内部Flash） ── */
#define TEMP_WARN_DEFAULT           35
#define HUM_WARN_DEFAULT            80

/* ── 烟雾自动校准偏移量默认值（MQ2，ADC值） ──
 * 动态阈值 = 预热基准线 + 偏移量（相对差值判定，适配不同模块基准）
 * 一级预警：基准线 + SmokeWarnOffset
 * 二级报警：基准线 + SmokeAlarmOffset
 * 三级紧急：基准线 + SmokeEmergencyOffset */
#define SMOKE_WARN_OFFSET_DEFAULT       200   /* 一级：预警（浓度偏高） */
#define SMOKE_ALARM_OFFSET_DEFAULT      300   /* 二级：报警（判定有烟雾） */
#define SMOKE_EMERGENCY_OFFSET_DEFAULT  500   /* 三级：紧急（继电器断电保护） */

/* ── 阈值调节范围与步进 ── */
#define TEMP_WARN_MIN       20
#define TEMP_WARN_MAX       60
#define HUM_WARN_MIN        30
#define HUM_WARN_MAX        95
#define SMOKE_OFFSET_MIN    50
#define SMOKE_OFFSET_MAX    1000
#define SMOKE_OFFSET_STEP   20

/* ── 阈值配置结构体（保存到Flash，掉电不丢失） ── */
typedef struct {
    uint8_t  TempWarn;             /* 温度预警阈值 (°C) */
    uint8_t  HumWarn;              /* 湿度预警阈值 (%) */
    uint16_t SmokeWarnOffset;      /* 烟雾一级预警偏移 (ADC) */
    uint16_t SmokeAlarmOffset;     /* 烟雾二级报警偏移 (ADC) */
    uint16_t SmokeEmergencyOffset; /* 烟雾三级紧急偏移 (ADC) */
} ThresholdConfigTypeDef;

/* ── MQ2上电预热时间：加热器需预热，期间不判定烟雾，避免刚上电误报 ── */
#define SMOKE_WARMUP_MS  60000

/* ── 人体检测自动开灯：人离开后LED保持点亮时间 ── */
#define PIR_LED_HOLD_MS  5000

/* ── 设备状态枚举 ── */
typedef enum {
    STATE_NORMAL   = 0,
    STATE_WARNING  = 1,
    STATE_DANGER   = 2
} DeviceStateTypeDef;

/* ── 远程手动控制保持配置 ──
 * 除继电器外（继电器手动控制不超时，一直保持），其余执行器5秒后恢复自动 */
#define MANUAL_OVERRIDE_MS          5000
#define MANUAL_OVERRIDE_RELAY_MASK  0x01
#define MANUAL_OVERRIDE_BUZZER_MASK 0x02
#define MANUAL_OVERRIDE_LED_MASK    0x04
#define MANUAL_OVERRIDE_RGB_MASK    0x08
#define MANUAL_RGB_RED              0
#define MANUAL_RGB_GREEN            1
#define MANUAL_RGB_BLUE             2
#define MANUAL_RGB_OFF              3

/* ── 传感器数据结构体 ── */
typedef struct {
    uint8_t   Humidity;       /* DHT11湿度 (0-100%) */
    uint8_t   Temperature;    /* DHT11温度 (0-50°C) */
    uint16_t  SmokeValue;     /* MQ2烟雾传感器ADC值 */
    uint8_t   SmokeFlag;      /* 烟雾检测标志 (0:无烟雾, 1:有烟雾) */
    uint8_t   SmokeReady;     /* MQ2预热完成标志 (0:预热中, 1:可判定烟雾) */
    uint16_t  SmokeBaseline;  /* MQ2自动校准基准线（预热期间洁净空气均值） */
    uint16_t  FlameValue;     /* 火焰传感器ADC值 */
    uint8_t   FlameFlag;      /* 火焰检测标志 (0:无火焰, 1:有火焰) */
    uint8_t   PirFlag;        /* 人体检测标志 (0:无人, 1:检测到人体) */
} SensorDataStruct;

/* ── 设备状态结构体 ── */
typedef struct {
    DeviceStateTypeDef State;   /* 当前设备状态 */
    uint8_t            MQTTConnected; /* MQTT连接状态 */
    char               AiResultText[64]; /* AI分析结果 */
    uint8_t            AiResultNew;      /* 新AI结果标志 */
    uint8_t            ManualOverrideMask; /* 远程手动控制有效位 */
    uint16_t           ManualOverrideMs;   /* 手动控制剩余保持时间 */
    uint8_t            ManualRelay;        /* 远程继电器目标状态 */
    uint8_t            ManualBuzzer;       /* 远程蜂鸣器目标状态 */
    uint8_t            ManualLed;          /* 远程LED目标状态 */
    uint8_t            ManualRgb;          /* 远程RGB目标颜色 */
} DeviceStatusStruct;

/* ── 全局数据共享区 ── */
extern SensorDataStruct    g_SensorData;    /* 传感器数据 */
extern DeviceStatusStruct  g_DeviceStatus;  /* 设备状态 */
extern ThresholdConfigTypeDef g_Threshold;  /* 阈值配置（按键可调，存Flash） */
extern volatile uint8_t    g_KeyShort;      /* 最近一次短按：1/2/3对应KEY1/2/3，消费后清零 */
extern volatile uint8_t    g_KeyLong;       /* 最近一次长按(≥1s)：1/2/3，消费后清零 */
extern volatile uint8_t    g_DisplayPage;   /* OLED页面：0=数据页，1=阈值设置页 */
extern volatile uint8_t    g_BuzzerMute;    /* 1=报警消音（仅WARNING生效，DANGER不消音） */
extern volatile uint8_t    g_FanManual;     /* 1=手动开启风扇（继电器） */

/* ── FreeRTOS信号量句柄 ── */
extern SemaphoreHandle_t   xSemaphoreSensor;    /* 传感器数据访问信号量 */
extern SemaphoreHandle_t   xSemaphoreStatus;    /* 设备状态访问信号量 */
extern SemaphoreHandle_t   xSemaphoreOLED;      /* OLED显示互斥信号量 */

/* ── 任务优先级定义 ── */
#define PRIORITY_SENSOR        4
#define PRIORITY_CONTROL       3
#define PRIORITY_DISPLAY       2
#define PRIORITY_MQTT          1
#define PRIORITY_KEY           2

/* ── 任务堆栈大小定义 ── */
#define STACK_SENSOR           256
#define STACK_CONTROL          256
#define STACK_DISPLAY          256
#define STACK_MQTT             512
#define STACK_KEY              128

#endif /* __APPCONFIG_H */
