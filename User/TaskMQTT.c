#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "../System/Delay.h"
#include "../Hardware/USART.h"
#include "../Hardware/MQTT.h"
#include "../Hardware/CmdParser.h"
#include "../Hardware/Storage.h"
#include "AppConfig.h"

/* ── MQTT连接测试步骤（与原项目一致，使用全局变量） ── */
uint8_t TestStep = 0;

/* ── MQTT连接健康监测 ── */
#define MQTT_MAX_PING_FAIL  3   /* 连续3次Ping失败则触发重连 */
static uint8_t s_PingFailCount = 0;

/* ── 历史数据掉电保存（每30秒一条，写内部Flash） ── */
#define STORAGE_INTERVAL_MS  30000
static uint16_t s_StorageTimer = 0;

/* ── 重置MQTT连接（触发重连流程） ── */
static void MQTT_ResetConnection(void)
{
    TestStep = 2;  /* 从WiFi重连步骤开始（跳过AT测试和模式设置） */
    s_PingFailCount = 0;
    if(xSemaphoreTake(xSemaphoreStatus, portMAX_DELAY) == pdTRUE)
    {
        g_DeviceStatus.MQTTConnected = 0;
        xSemaphoreGive(xSemaphoreStatus);
    }
}

static void MQTT_PublishSensorDataNow(void)
{
    uint8_t temperature = 0;
    uint8_t humidity = 0;
    uint16_t smokeValue = 0;
    uint8_t smokeFlag = 0;
    uint8_t flameFlag = 0;
    DeviceStateTypeDef deviceState = STATE_NORMAL;
    char DataPayload[256];

    if(xSemaphoreTake(xSemaphoreSensor, portMAX_DELAY) == pdTRUE)
    {
        temperature = g_SensorData.Temperature;
        humidity = g_SensorData.Humidity;
        smokeValue = g_SensorData.SmokeValue;
        smokeFlag = g_SensorData.SmokeFlag;
        flameFlag = g_SensorData.FlameFlag;
        xSemaphoreGive(xSemaphoreSensor);
    }

    if(xSemaphoreTake(xSemaphoreStatus, portMAX_DELAY) == pdTRUE)
    {
        deviceState = g_DeviceStatus.State;
        xSemaphoreGive(xSemaphoreStatus);
    }

    /* shock是0/1状态量；smoke_adc额外上报MQ2原始ADC数值，用于小程序显示曲线。 */
    sprintf(DataPayload,
            "{\"id\":\"1\",\"version\":\"1.0\",\"params\":{\"temp\":{\"value\":%d},\"hum\":{\"value\":%d},\"flame\":{\"value\":%d},\"shock\":{\"value\":%d},\"smoke_adc\":{\"value\":%u},\"state\":{\"value\":%d}}}",
            temperature, humidity, flameFlag, smokeFlag, (unsigned int)smokeValue, deviceState);
    MQTT_Publish(MQTT_TOPIC_DATA, DataPayload);
}

/* ── 保存历史数据到内部Flash（掉电不丢失） ── */
static void MQTT_SaveHistoryData(void)
{
    StorageRecordTypeDef rec;
    uint8_t temperature = 0;
    uint8_t humidity = 0;
    uint16_t smokeValue = 0;

    if(xSemaphoreTake(xSemaphoreSensor, portMAX_DELAY) == pdTRUE)
    {
        temperature = g_SensorData.Temperature;
        humidity = g_SensorData.Humidity;
        smokeValue = g_SensorData.SmokeValue;
        xSemaphoreGive(xSemaphoreSensor);
    }

    rec.Timestamp   = (uint32_t)(xTaskGetTickCount() / configTICK_RATE_HZ);  /* 上电运行秒数 */
    rec.Temperature = temperature;
    rec.Humidity    = humidity;
    rec.SmokeValue  = smokeValue;

    Storage_Append(&rec);
}

/* ── MQTT通信任务 ── */
void TaskMQTT(void *pvParameters)
{
    TickType_t xLastWakeTime;
    uint16_t MQTTTimer = 0;
    uint16_t DataTimer = 0;
    uint8_t FirstInit = 0;
    
    xLastWakeTime = xTaskGetTickCount();
    
    while(1)
    {
        /* 10ms周期执行 */
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(10));
        
        /* ── 获取MQTT连接状态 ── */
        uint8_t mqttConnected = 0;
        if(xSemaphoreTake(xSemaphoreStatus, portMAX_DELAY) == pdTRUE)
        {
            mqttConnected = g_DeviceStatus.MQTTConnected;
            xSemaphoreGive(xSemaphoreStatus);
        }
        
        /* ── MQTT未连接状态处理 ── */
        if(mqttConnected == 0)
        {
            MQTTTimer += 10;
            if(MQTTTimer >= 3000)
            {
                MQTTTimer = 0;
                
                if(FirstInit == 0)
                {
                    FirstInit = 1;
                    MQTT_Init();
                    
                    /* ── 尝试从透传模式唤醒ESP8266 ── */
                    vTaskDelay(1500);
                    USART1_ClearRxBuffer();
                    USART1_SendString("+++");          /* 无CR/LF！ */
                    vTaskDelay(1500);
                    USART1_ClearRxBuffer();
                    USART1_SendString("AT\r\n");       /* 检查是否退出成功 */
                    USART1_WaitForResponse("OK", 1000);
                    /* 硬复位ESP8266 */
                    USART1_ClearRxBuffer();
                    USART1_SendString("AT+RST\r\n");
                    USART1_WaitForResponse("OK", 1000);
                    vTaskDelay(3000);  /* 等待ESP8266重启 */
                }
                
                char Cmd[128];
                uint8_t retry;
                
                switch(TestStep)
                {
                    case 0:  /* Step 0 → AT测试 */
                        for(retry = 0; retry < 5; retry++)
                        {
                            USART1_ClearRxBuffer();
                            USART1_SendString("AT\r\n");
                            if(USART1_WaitForResponse("OK", 2000) == 1)
                            {
                                TestStep = 1;
                                break;
                            }
                            vTaskDelay(1000);
                        }
                        break;
                        
                    case 1:  /* Step 1 → 设置WiFi模式 */
                        USART1_ClearRxBuffer();
                        USART1_SendString("AT+CWMODE=1\r\n");
                        if(USART1_WaitForResponse("OK", 2000) == 1)
                        {
                            TestStep = 2;
                        }
                        break;
                        
                    case 2:  /* Step 2 → 连接WiFi */
                        USART1_ClearRxBuffer();
                        sprintf(Cmd, "AT+CWJAP=\"%s\",\"%s\"\r\n", WIFI_SSID, WIFI_PASS);
                        USART1_SendString(Cmd);
                        if(USART1_WaitForResponse("OK", 8000) == 1)
                        {
                            TestStep = 3;
                        }
                        break;
                        
                    case 3:  /* Step 3 → MQTT连接（TCP+协议+订阅，由MQTT_Connect统一处理） */
                        if(MQTT_Connect() == 1)
                        {
                            TestStep = 4;
                            if(xSemaphoreTake(xSemaphoreStatus, portMAX_DELAY) == pdTRUE)
                            {
                                g_DeviceStatus.MQTTConnected = 1;
                                xSemaphoreGive(xSemaphoreStatus);
                            }
                        }

                        break;
                        
                    case 4:  /* MQTT已连接 */
                        break;
                }
            }
        }
        
        /* ── 历史数据保存（每30秒一条，写内部Flash，掉电不丢失）
         * 注意：不依赖MQTT连接状态，离线也照常保存 ── */
        s_StorageTimer += 10;
        if(s_StorageTimer >= STORAGE_INTERVAL_MS)
        {
            s_StorageTimer = 0;
            MQTT_SaveHistoryData();
        }
        
        /* ── MQTT已连接状态处理 ── */
        if(mqttConnected == 1)
        {
            /* ── 心跳保活（与原项目一致，双定时器：30s和20s） ── */
            {
                static uint16_t KeepAliveTimer = 0;
                KeepAliveTimer += 10;
                if(KeepAliveTimer >= 30000)
                {
                    KeepAliveTimer = 0;
                    if(MQTT_Ping() == 0)
                    {
                        s_PingFailCount++;
                        if(s_PingFailCount >= MQTT_MAX_PING_FAIL)
                            MQTT_ResetConnection();
                    }
                    else
                    {
                        s_PingFailCount = 0;
                    }
                }
            }
            
            /* ── 强制IPD_Ready检测 ── */
            {
                static uint8_t prevS = 0;
                if(USART1_SaveIPDCount != prevS)
                {
                    prevS = USART1_SaveIPDCount;
                    IPD_Ready = 1;
                }
            }
            
            /* ── 处理接收到的MQTT数据 ── */
            if(MQTT_ProcessRx() == 1)
            {
                CmdTypeDef Cmd = CmdParser_Parse(MQTT_RxPayload);
                if(Cmd != CMD_NONE)
                {
                    CmdParser_Execute(Cmd);
                    if(CurrentCmd.IsOneNetService == 0)
                    {
                        vTaskDelay(100);
                        MQTT_PublishSensorDataNow();
                    }
                }
                MQTT_ClearRxBuffer();
            }
            
            /* ── 数据发布（每5秒一次） ── */
            DataTimer += 10;
            if(DataTimer >= 5000)
            {
                DataTimer = 0;
                
                MQTT_PublishSensorDataNow();
                vTaskDelay(200);
            }
            
            
            /* ── 第二个Ping定时器（与原项目一致，20s） ── */
            {
                static uint16_t PingTimer = 0;
                PingTimer += 10;
                if(PingTimer >= 20000)
                {
                    PingTimer = 0;
                    if(MQTT_Ping() == 0)
                    {
                        s_PingFailCount++;
                        if(s_PingFailCount >= MQTT_MAX_PING_FAIL)
                            MQTT_ResetConnection();
                    }
                    else
                    {
                        s_PingFailCount = 0;
                    }
                }
            }
        }
    }
}
