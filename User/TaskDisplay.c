#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "../Hardware/OLED.h"
#include "../Hardware/MQTT.h"
#include "../Hardware/Flame.h"
#include "../Hardware/Storage.h"
#include "AppConfig.h"

/* ── 上一次显示的值（仅变化时刷新，消除闪烁） ── */
static uint16_t s_LastSmoke    = 0xFFFF;
static uint8_t  s_LastTemp     = 0xFF;
static uint8_t  s_LastHum      = 0xFF;
static uint16_t s_LastFlame    = 0xFFFF;
static uint8_t  s_LastPir      = 0xFF;
static uint8_t  s_LastState    = 0xFF;
static uint8_t  s_LastNetStatus = 0xFF;
static uint8_t  s_LastIndicator = 0xFF;
static uint16_t s_LastStored   = 0xFFFF;
static uint8_t  s_FirstRun     = 1;

/* ── 页面/设置状态 ── */
static uint8_t  s_Page         = 0;    /* 当前页：0=数据页，1=阈值设置页 */
static uint8_t  s_SettingsItem = 0;    /* 设置项：0=温度 1=湿度 2=烟雾预警 3=烟雾报警 4=烟雾紧急 */
static uint8_t  s_SettingsDirty = 0;   /* 设置有改动，退出时保存 */
static uint16_t s_SettingsIdle = 0;    /* 设置页无操作计时（100ms单位） */

#define SETTINGS_IDLE_EXIT   300       /* 设置页30秒无操作自动保存退出 */

/* ── 数据页固定标签（只写一次，永不擦除） ── */
static void Page0_PaintStatic(void)
{
    OLED_ShowString(1, 1, "Stored:");
    OLED_ShowString(2, 1, "Smoke:");
    OLED_ShowString(2, 12, "P:");
    OLED_ShowString(3, 1, "T:");
    OLED_ShowChar(3, 5, 'C');
    OLED_ShowString(3, 7, "H:");
    OLED_ShowChar(3, 11, '%');
    OLED_ShowString(4, 1, "Flame:");
    OLED_ShowString(4, 11, "St:");
}

/* ── 设置页：重绘当前项名和值 ── */
static void Settings_ShowItem(void)
{
    switch(s_SettingsItem)
    {
        case 0:  OLED_ShowString(1, 5, "Temp "); break;
        case 1:  OLED_ShowString(1, 5, "Hum  "); break;
        case 2:  OLED_ShowString(1, 5, "S.Wn "); break;
        case 3:  OLED_ShowString(1, 5, "S.Al "); break;
        default: OLED_ShowString(1, 5, "S.Em "); break;
    }

    if(s_SettingsItem <= 1)
    {
        uint8_t v = (s_SettingsItem == 0) ? g_Threshold.TempWarn : g_Threshold.HumWarn;
        OLED_ShowNum(2, 5, v, 2);
        OLED_ShowChar(2, 7, (s_SettingsItem == 0) ? 'C' : '%');
        OLED_ShowString(2, 8, "        ");
    }
    else
    {
        uint16_t v = (s_SettingsItem == 2) ? g_Threshold.SmokeWarnOffset :
                     (s_SettingsItem == 3) ? g_Threshold.SmokeAlarmOffset :
                                             g_Threshold.SmokeEmergencyOffset;
        OLED_ShowNum(2, 5, v, 4);
        OLED_ShowString(2, 9, "      ");
    }
}

/* ── 设置页：整屏重绘 ── */
static void Settings_Paint(void)
{
    OLED_Clear();
    OLED_ShowString(1, 1, "Set:");
    OLED_ShowString(2, 1, "Val:");
    OLED_ShowString(3, 1, "K1:- K2:+ K3:next");
    OLED_ShowString(4, 1, "HoldK3:save exit");
    Settings_ShowItem();
}

/* ── 设置页：当前项加减（带范围钳制） ── */
static void Settings_Adjust(int16_t delta)
{
    int16_t v;

    switch(s_SettingsItem)
    {
        case 0:
            v = (int16_t)g_Threshold.TempWarn + delta;
            if(v < TEMP_WARN_MIN) v = TEMP_WARN_MIN;
            if(v > TEMP_WARN_MAX) v = TEMP_WARN_MAX;
            g_Threshold.TempWarn = (uint8_t)v;
            break;

        case 1:
            v = (int16_t)g_Threshold.HumWarn + delta;
            if(v < HUM_WARN_MIN) v = HUM_WARN_MIN;
            if(v > HUM_WARN_MAX) v = HUM_WARN_MAX;
            g_Threshold.HumWarn = (uint8_t)v;
            break;

        case 2:
        case 3:
        case 4:
        {
            uint16_t *p = (s_SettingsItem == 2) ? &g_Threshold.SmokeWarnOffset :
                          (s_SettingsItem == 3) ? &g_Threshold.SmokeAlarmOffset :
                                                  &g_Threshold.SmokeEmergencyOffset;
            v = (int16_t)*p + delta;
            if(v < SMOKE_OFFSET_MIN) v = SMOKE_OFFSET_MIN;
            if(v > SMOKE_OFFSET_MAX) v = SMOKE_OFFSET_MAX;
            *p = (uint16_t)v;
            break;
        }
    }

    s_SettingsDirty = 1;
    Settings_ShowItem();
}

/* ── 退出设置页（可选保存）并回到数据页 ── */
static void Settings_Exit(uint8_t save)
{
    if(save && s_SettingsDirty)
        Storage_ConfigSave();

    s_SettingsDirty = 0;
    s_SettingsIdle = 0;
    s_Page = 0;
    g_DisplayPage = 0;

    OLED_Clear();
    Page0_PaintStatic();
    s_FirstRun = 1;   /* 强制刷新所有数据区 */
}

/* ── 数据页第1行右部状态指示：FAN(手动风扇) / MUT(消音) / P0 ── */
static void Page0_ShowIndicator(void)
{
    uint8_t ind = 0;

    if(g_FanManual)     ind = 2;
    else if(g_BuzzerMute) ind = 1;

    if(ind != s_LastIndicator || s_FirstRun)
    {
        s_LastIndicator = ind;
        if(ind == 2)      OLED_ShowString(1, 11, "FAN");
        else if(ind == 1) OLED_ShowString(1, 11, "MUT");
        else              OLED_ShowString(1, 11, "P0 ");
    }
}

/* ── OLED显示任务 ── */
void TaskDisplay(void *pvParameters)
{
    TickType_t xLastWakeTime;
    uint16_t DisplayTimer = 0;

    (void)pvParameters;

    OLED_Init();
    OLED_Clear();
    Page0_PaintStatic();

    xLastWakeTime = xTaskGetTickCount();

    while(1)
    {
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100));

        /* ── 按键事件消费 ── */
        {
            uint8_t ks = g_KeyShort;
            uint8_t kl = g_KeyLong;
            g_KeyShort = 0;
            g_KeyLong = 0;

            if(ks != 0 || kl != 0)
            {
                if(s_Page == 0)
                {
                    if(ks == 1)
                    {
                        /* KEY1：报警消音切换 */
                        g_BuzzerMute = !g_BuzzerMute;
                    }
                    else if(ks == 2)
                    {
                        /* KEY2：手动风扇切换 */
                        g_FanManual = !g_FanManual;
                    }
                    else if(ks == 3 || kl == 3)
                    {
                        /* KEY3：进入阈值设置页 */
                        s_SettingsItem = 0;
                        s_SettingsDirty = 0;
                        s_SettingsIdle = 0;
                        s_Page = 1;
                        g_DisplayPage = 1;

                        if(xSemaphoreTake(xSemaphoreOLED, portMAX_DELAY) == pdTRUE)
                        {
                            Settings_Paint();
                            xSemaphoreGive(xSemaphoreOLED);
                        }
                    }

                    if(xSemaphoreTake(xSemaphoreOLED, portMAX_DELAY) == pdTRUE)
                    {
                        Page0_ShowIndicator();
                        xSemaphoreGive(xSemaphoreOLED);
                    }
                }
                else
                {
                    s_SettingsIdle = 0;

                    if(xSemaphoreTake(xSemaphoreOLED, portMAX_DELAY) == pdTRUE)
                    {
                        if(kl == 3)
                        {
                            /* 长按KEY3：保存并退出 */
                            Settings_Exit(1);
                        }
                        else if(ks == 3)
                        {
                            /* 短按KEY3：确认并切换下一项 */
                            s_SettingsItem = (s_SettingsItem + 1) % 5;
                            Settings_ShowItem();
                        }
                        else if(ks == 1)
                        {
                            /* KEY1：减小 */
                            Settings_Adjust(-(int16_t)((s_SettingsItem <= 1) ? 1 : SMOKE_OFFSET_STEP));
                        }
                        else if(ks == 2)
                        {
                            /* KEY2：增大 */
                            Settings_Adjust((s_SettingsItem <= 1) ? 1 : SMOKE_OFFSET_STEP);
                        }
                        xSemaphoreGive(xSemaphoreOLED);
                    }
                }
            }
        }

        /* ── 数据页：每500ms刷新数据区 ── */
        if(s_Page == 0)
        {
            DisplayTimer += 100;
            if(DisplayTimer >= 500)
            {
                DisplayTimer = 0;

                if(xSemaphoreTake(xSemaphoreOLED, portMAX_DELAY) == pdTRUE)
                {
                    /* ── 读取共享数据 ── */
                    uint8_t  temperature = 0;
                    uint8_t  humidity = 0;
                    uint16_t smokeValue = 0;
                    uint16_t flameValue = 0;
                    uint8_t  pirFlag = 0;
                    DeviceStateTypeDef deviceState = STATE_NORMAL;
                    uint8_t  mqttConnected = 0;

                    if(xSemaphoreTake(xSemaphoreSensor, portMAX_DELAY) == pdTRUE)
                    {
                        temperature   = g_SensorData.Temperature;
                        humidity      = g_SensorData.Humidity;
                        smokeValue    = g_SensorData.SmokeValue;
                        flameValue    = g_SensorData.FlameValue;
                        pirFlag       = g_SensorData.PirFlag;
                        xSemaphoreGive(xSemaphoreSensor);
                    }

                    if(xSemaphoreTake(xSemaphoreStatus, portMAX_DELAY) == pdTRUE)
                    {
                        deviceState   = g_DeviceStatus.State;
                        mqttConnected = g_DeviceStatus.MQTTConnected;
                        xSemaphoreGive(xSemaphoreStatus);
                    }

                    extern uint8_t TestStep;

                    /* ── Row 1：历史存储条数 + 状态指示 + 连接状态 ── */
                    {
                        uint16_t stored = Storage_GetCount();

                        if(stored != s_LastStored || s_FirstRun)
                        {
                            s_LastStored = stored;
                            OLED_ShowNum(1, 8, stored, 3);   /* 最多127条，3位显示 */
                        }

                        Page0_ShowIndicator();

                        uint8_t netStatus;

                        if(mqttConnected == 1 && TestStep == 4)
                            netStatus = 3;      /* All ready */
                        else if(TestStep == 3)
                            netStatus = 2;      /* MQTT connecting */
                        else if(TestStep == 2)
                            netStatus = 1;      /* WiFi connecting */
                        else
                            netStatus = 0;      /* ESP/AT preparing */

                        if(netStatus != s_LastNetStatus || s_FirstRun)
                        {
                            s_LastNetStatus = netStatus;

                            switch(netStatus)
                            {
                                case 3:  OLED_ShowString(1, 14, "OK"); break;
                                case 2:  OLED_ShowString(1, 14, "MQ"); break;
                                case 1:  OLED_ShowString(1, 14, "WF"); break;
                                default: OLED_ShowString(1, 14, "AT"); break;
                            }
                        }
                    }

                    /* ── Row 2：烟雾 + 人体检测 ── */
                    if(smokeValue != s_LastSmoke || pirFlag != s_LastPir || s_FirstRun)
                    {
                        s_LastSmoke = smokeValue;
                        s_LastPir = pirFlag;

                        OLED_ShowNum(2, 7, smokeValue, 4);

                        if(pirFlag == 1)
                            OLED_ShowString(2, 14, "ON ");
                        else
                            OLED_ShowString(2, 14, "OFF");
                    }

                    /* ── Row 3：温度 + 湿度（均来自DHT11） ── */
                    if(humidity != s_LastHum || temperature != s_LastTemp || s_FirstRun)
                    {
                        s_LastHum = humidity;
                        s_LastTemp = temperature;

                        if(temperature > 0 && temperature <= 60)
                        {
                            OLED_ShowNum(3, 3, temperature, 2);
                            OLED_ShowChar(3, 5, 'C');
                        }
                        else
                        {
                            OLED_ShowString(3, 3, "--");
                        }

                        if(humidity > 0 && humidity <= 100)
                        {
                            OLED_ShowNum(3, 9, humidity, 2);
                            OLED_ShowChar(3, 11, '%');
                        }
                        else
                        {
                            OLED_ShowString(3, 9, "--");
                        }
                    }

                    /* ── Row 4：火焰 + 状态 ── */
                    if(flameValue != s_LastFlame || deviceState != s_LastState || s_FirstRun)
                    {
                        s_LastFlame = flameValue;
                        s_LastState = deviceState;

                        /* 火焰状态固定4字符（列7-10），与"St:"(列12-13)不冲突 */
                        if(flameValue < FLAME_THRESHOLD)
                            OLED_ShowString(4, 7, "Fire");
                        else
                            OLED_ShowString(4, 7, "OK  ");

                        switch(deviceState)
                        {
                            case STATE_NORMAL:  OLED_ShowString(4, 14, "N"); break;
                            case STATE_WARNING: OLED_ShowString(4, 14, "W"); break;
                            case STATE_DANGER:  OLED_ShowString(4, 14, "D"); break;
                        }
                    }

                    s_FirstRun = 0;
                    xSemaphoreGive(xSemaphoreOLED);
                }
            }
        }
        else
        {
            /* ── 设置页：30秒无操作自动保存退出 ── */
            s_SettingsIdle++;
            if(s_SettingsIdle >= SETTINGS_IDLE_EXIT)
            {
                if(xSemaphoreTake(xSemaphoreOLED, portMAX_DELAY) == pdTRUE)
                {
                    Settings_Exit(1);
                    xSemaphoreGive(xSemaphoreOLED);
                }
            }
        }
    }
}
