#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "../Hardware/LED.h"
#include "../Hardware/Buzzer.h"
#include "../Hardware/Relay.h"
#include "../Hardware/RGB.h"
#include "AppConfig.h"

/* ── DANGER最小保持时间（防止ADC抖动导致状态机快速震荡） ── */
#define DANGER_HOLD_MS  2000  /* 进入DANGER后至少保持2秒 */

/* ── 状态切换控制变量 ── */
static DeviceStateTypeDef s_PrevState = STATE_NORMAL;
static uint16_t s_DangerHoldTimer = 0;

/* ── WARNING状态闪烁定时器 ── */
static uint16_t s_BlueTimer = 0;
static uint8_t  s_BlueOn = 0;
static uint16_t s_BuzzerTimer = 0;

/* ── DANGER状态闪烁定时器 ── */
static uint16_t s_RgbTimer = 0;
static uint8_t  s_RgbOn = 0;

/* ── 人体检测自动开灯（PWM渐亮渐灭） ── */
#define LED_FADE_STEP   3       /* 每10ms变化3%，约330ms完成渐亮/渐灭 */
static uint8_t  s_LedDuty      = 0;    /* 当前亮度 0~100 */
static uint8_t  s_LedTarget    = 0;    /* 目标亮度 0~100 */
static uint16_t s_PirOffTimer  = 0;    /* 人离开后保持点亮计时 */

/* ── 状态切换时重置所有定时器 ── */
static void ResetStateTimers(void)
{
    s_BlueTimer = 0;
    s_BlueOn = 0;
    s_BuzzerTimer = 0;
    s_RgbTimer = 0;
    s_RgbOn = 0;
    s_DangerHoldTimer = 0;
}

static void ApplyManualOverride(DeviceStateTypeDef currentState)
{
    uint8_t mask = 0;       /* 非继电器手动控制位（5秒超时） */
    uint8_t relayMask = 0;  /* 继电器手动控制位（不超时，一直保持） */
    uint8_t relay = 0;
    uint8_t buzzer = 0;
    uint8_t led = 0;
    uint8_t rgb = MANUAL_RGB_OFF;

    if(xSemaphoreTake(xSemaphoreStatus, portMAX_DELAY) == pdTRUE)
    {
        if(currentState == STATE_DANGER)
        {
            g_DeviceStatus.ManualOverrideMask = 0;
            g_DeviceStatus.ManualOverrideMs = 0;
            xSemaphoreGive(xSemaphoreStatus);
            return;
        }

        /* 继电器手动控制常保持：一旦开启/关闭，一直保持到用户再次操作或进入DANGER */
        relayMask = g_DeviceStatus.ManualOverrideMask & MANUAL_OVERRIDE_RELAY_MASK;
        relay = g_DeviceStatus.ManualRelay;

        if(g_DeviceStatus.ManualOverrideMs > 10)
        {
            g_DeviceStatus.ManualOverrideMs -= 10;
            mask = g_DeviceStatus.ManualOverrideMask & ~MANUAL_OVERRIDE_RELAY_MASK;
            buzzer = g_DeviceStatus.ManualBuzzer;
            led = g_DeviceStatus.ManualLed;
            rgb = g_DeviceStatus.ManualRgb;
        }
        else
        {
            /* 超时：仅清除非继电器手动控制位，继电器保持 */
            g_DeviceStatus.ManualOverrideMask = relayMask;
            g_DeviceStatus.ManualOverrideMs = 0;
        }

        xSemaphoreGive(xSemaphoreStatus);
    }

    /* 继电器手动控制（无超时） */
    if(relayMask)
    {
        if(relay) Relay_Open();
        else Relay_Close();
    }

    if(mask == 0)
        return;

    if(mask & MANUAL_OVERRIDE_BUZZER_MASK)
    {
        if(buzzer) Buzzer_On();
        else Buzzer_Off();
    }

    if(mask & MANUAL_OVERRIDE_LED_MASK)
    {
        if(led) { LED_On(); s_LedDuty = 100; }
        else    { LED_Off(); s_LedDuty = 0; }
    }

    if(mask & MANUAL_OVERRIDE_RGB_MASK)
    {
        if(rgb == MANUAL_RGB_RED) RGB_Red();
        else if(rgb == MANUAL_RGB_GREEN) RGB_Green();
        else if(rgb == MANUAL_RGB_BLUE) RGB_Blue();
        else RGB_Off();
    }
}

/* ── 更新设备状态 ── */
static void UpdateState(void)
{
    uint8_t TempAlarm = 0;
    uint8_t HumAlarm = 0;
    DeviceStateTypeDef newState;
    uint8_t  tempWarn = g_Threshold.TempWarn;          /* 运行时阈值（按键可调） */
    uint8_t  humWarn = g_Threshold.HumWarn;
    uint16_t smokeWarn = g_Threshold.SmokeWarnOffset;
    uint16_t smokeAlarm = g_Threshold.SmokeAlarmOffset;
    uint16_t smokeEmergency = g_Threshold.SmokeEmergencyOffset;

    if(xSemaphoreTake(xSemaphoreSensor, portMAX_DELAY) == pdTRUE)
    {
        TempAlarm = ((int)g_SensorData.Temperature >= tempWarn);
        HumAlarm = (g_SensorData.Humidity >= humWarn);

        /* 三级状态判定（烟雾用相对基准线的差值判定）：
         *  火焰 → DANGER（最高优先级）
         *  烟雾三级（预热完成且差值≥烟雾紧急偏移）→ DANGER
         *  烟雾二级（预热完成且差值≥烟雾报警偏移）→ DANGER
         *  烟雾一级（预热完成且差值≥烟雾预警偏移，浓度偏高）→ WARNING
         *  温湿度超限 → WARNING
         *  其余 → NORMAL
         *  注意：预热完成前（SmokeReady=0）不按烟雾值判级，避免刚上电误报 */
        {
            uint16_t smokeDelta = 0;
            if(g_SensorData.SmokeBaseline > 0 && g_SensorData.SmokeValue > g_SensorData.SmokeBaseline)
                smokeDelta = g_SensorData.SmokeValue - g_SensorData.SmokeBaseline;

            if(g_SensorData.FlameFlag == 1)
                newState = STATE_DANGER;
            else if(g_SensorData.SmokeReady == 1 && smokeDelta >= smokeEmergency)
                newState = STATE_DANGER;
            else if(g_SensorData.SmokeReady == 1 && smokeDelta >= smokeAlarm)
                newState = STATE_DANGER;
            else if(g_SensorData.SmokeReady == 1 && smokeDelta >= smokeWarn)
                newState = STATE_WARNING;
            else if(TempAlarm == 1 || HumAlarm == 1)
                newState = STATE_WARNING;
            else
                newState = STATE_NORMAL;
        }
        xSemaphoreGive(xSemaphoreSensor);
    }
    else
    {
        return;
    }
    
    if(xSemaphoreTake(xSemaphoreStatus, portMAX_DELAY) == pdTRUE)
    {
        /* DANGER最小保持时间：进入DANGER后保持至少DANGER_HOLD_MS */
        if(g_DeviceStatus.State == STATE_DANGER)
        {
            s_DangerHoldTimer += 10;
            if(newState != STATE_DANGER && s_DangerHoldTimer < DANGER_HOLD_MS)
            {
                /* 保持时间未到，不切换 */
                xSemaphoreGive(xSemaphoreStatus);
                return;
            }
        }
        
        /* 检测到状态变化或新状态与之前不同时重置定时器 */
        if(newState != g_DeviceStatus.State)
        {
            ResetStateTimers();
        }
        
        if(newState != s_PrevState)
        {
            ResetStateTimers();
        }
        
        g_DeviceStatus.State = newState;
        s_PrevState = newState;
        xSemaphoreGive(xSemaphoreStatus);
    }
}

/* ── 状态控制任务 ── */
void TaskControl(void *pvParameters)
{
    TickType_t xLastWakeTime;
    
    /* 初始化硬件 */
    LED_Init();
    Buzzer_Init();
    Relay_Init();
    RGB_Init();
    
    /* 初始化状态 */
    if(xSemaphoreTake(xSemaphoreStatus, portMAX_DELAY) == pdTRUE)
    {
        g_DeviceStatus.State = STATE_NORMAL;
        xSemaphoreGive(xSemaphoreStatus);
    }
    
    xLastWakeTime = xTaskGetTickCount();
    
    while(1)
    {
        /* 10ms周期执行 */
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(10));
        
        /* ── 更新设备状态 ── */
        UpdateState();
        
        /* ── 获取当前状态 ── */
        DeviceStateTypeDef currentState = STATE_NORMAL;
        if(xSemaphoreTake(xSemaphoreStatus, portMAX_DELAY) == pdTRUE)
        {
            currentState = g_DeviceStatus.State;
            xSemaphoreGive(xSemaphoreStatus);
        }
        
        /* ── 根据状态执行控制逻辑 ── */
        switch(currentState)
        {
            case STATE_NORMAL:
                RGB_Green();
                Buzzer_Off();
                g_BuzzerMute = 0;   /* 状态恢复正常时自动解除消音 */
                if(g_FanManual)
                    Relay_Open();   /* 手动风扇开启 */
                else
                    Relay_Close();
                break;
                
            case STATE_WARNING:
                Relay_Open();
                s_BlueTimer += 10;
                if(s_BlueTimer >= 500)
                {
                    s_BlueTimer = 0;
                    s_BlueOn = !s_BlueOn;
                    if(s_BlueOn) RGB_Blue();
                    else RGB_Off();
                }
                s_BuzzerTimer += 10;
                if(s_BuzzerTimer <= 200)
                {
                    if(g_BuzzerMute == 0) Buzzer_On();   /* 消音时不响 */
                }
                else if(s_BuzzerTimer >= 1000) s_BuzzerTimer = 0;
                else Buzzer_Off();
                break;
                
            case STATE_DANGER:
                Buzzer_On();   /* DANGER不可消音，安全优先 */
                /* 三级紧急（差值达紧急偏移）：继电器断电保护；二级报警：继电器开（通风） */
                {
                    uint16_t smokeNow = 0;
                    uint16_t smokeBase = 0;
                    uint8_t smokeReady = 0;
                    uint16_t smokeEmergency = g_Threshold.SmokeEmergencyOffset;
                    if(xSemaphoreTake(xSemaphoreSensor, portMAX_DELAY) == pdTRUE)
                    {
                        smokeNow = g_SensorData.SmokeValue;
                        smokeBase = g_SensorData.SmokeBaseline;
                        smokeReady = g_SensorData.SmokeReady;
                        xSemaphoreGive(xSemaphoreSensor);
                    }
                    if(smokeReady == 1 && smokeBase > 0 &&
                       smokeNow >= smokeBase && (smokeNow - smokeBase) >= smokeEmergency)
                        Relay_Close();
                    else
                        Relay_Open();
                }
                s_RgbTimer += 10;
                if(s_RgbTimer >= 100)
                {
                    s_RgbTimer = 0;
                    s_RgbOn = !s_RgbOn;
                    if(s_RgbOn) RGB_Red();
                    else RGB_Off();
                }
                break;
        }
        
        /* ── 人体检测自动开灯：人来渐亮，人走延时后渐灭；危险状态强制点亮 ── */
        {
            uint8_t pir = 0;
            uint8_t manualLedActive = 0;
            if(xSemaphoreTake(xSemaphoreSensor, portMAX_DELAY) == pdTRUE)
            {
                pir = g_SensorData.PirFlag;
                xSemaphoreGive(xSemaphoreSensor);
            }
            if(xSemaphoreTake(xSemaphoreStatus, portMAX_DELAY) == pdTRUE)
            {
                manualLedActive = ((g_DeviceStatus.ManualOverrideMask & MANUAL_OVERRIDE_LED_MASK) != 0) &&
                                  (g_DeviceStatus.ManualOverrideMs > 10);
                xSemaphoreGive(xSemaphoreStatus);
            }

            /* 目标亮度：危险强制全亮；有人全亮；无人保持PIR_LED_HOLD_MS后熄灭 */
            if(currentState == STATE_DANGER)
            {
                s_LedTarget = 100;
            }
            else if(pir == 1)
            {
                s_LedTarget = 100;
                s_PirOffTimer = 0;
            }
            else
            {
                if(s_LedTarget == 100)
                {
                    s_PirOffTimer += 10;
                    if(s_PirOffTimer >= PIR_LED_HOLD_MS)
                    {
                        s_LedTarget = 0;
                        s_PirOffTimer = 0;
                    }
                }
            }

            /* 远程手动控制期间LED由指令直接控制，渐亮渐灭暂停 */
            if(manualLedActive == 0)
            {
                if(s_LedDuty < s_LedTarget)
                {
                    s_LedDuty += LED_FADE_STEP;
                    if(s_LedDuty > s_LedTarget) s_LedDuty = s_LedTarget;
                }
                else if(s_LedDuty > s_LedTarget)
                {
                    if(s_LedDuty > LED_FADE_STEP) s_LedDuty -= LED_FADE_STEP;
                    else s_LedDuty = 0;
                    if(s_LedDuty < s_LedTarget) s_LedDuty = s_LedTarget;
                }
                LED_SetDuty(s_LedDuty);
            }
        }

        ApplyManualOverride(currentState);
    }
}
