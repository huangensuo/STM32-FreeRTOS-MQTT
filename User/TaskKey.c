#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "../Hardware/Key.h"
#include "AppConfig.h"

/* ── 长按判定时间 ── */
#define KEY_LONG_MS  1000

/* ── 按键任务 ──
 * KEY1: PB5  -> GND
 * KEY2: PB6  -> GND
 * KEY3: PB7  -> GND
 * 按下时间 < 1s 松开 → g_KeyShort 事件
 * 按下时间 >= 1s 松开 → g_KeyLong  事件
 * 事件由显示任务消费，消费后清零。 */
void TaskKey(void *pvParameters)
{
    uint8_t rawMask = 0;
    uint8_t inPress = 0;
    uint8_t pressKey = 0;
    TickType_t pressStart = 0;

    (void)pvParameters;

    Key_Init();

    while(1)
    {
        rawMask = Key_GetRawMask();

        /* 边沿检测：未按下 → 按下 */
        if(rawMask != 0 && inPress == 0)
        {
            inPress = 1;
            pressStart = xTaskGetTickCount();

            if(rawMask & 0x01)      pressKey = 1;
            else if(rawMask & 0x02) pressKey = 2;
            else                    pressKey = 3;
        }
        /* 边沿检测：按下 → 松开，按持续时间产生事件 */
        else if(rawMask == 0 && inPress == 1)
        {
            inPress = 0;

            if((xTaskGetTickCount() - pressStart) >= pdMS_TO_TICKS(KEY_LONG_MS))
                g_KeyLong = pressKey;   /* 长按事件（新事件覆盖旧事件） */
            else
                g_KeyShort = pressKey;  /* 短按事件 */
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
