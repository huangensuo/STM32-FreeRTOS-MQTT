#include "LLM.h"
#include "WiFi.h"
#include "USART.h"
#include "../System/Delay.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
#include <stdio.h>

LLM_StatusTypeDef LLM_Status = LLM_STATUS_IDLE;
char LLM_Response[LLM_MAX_RESPONSE];

uint8_t LLM_Init(void)
{
    if(WIFI_Status != WIFI_STATUS_CONNECTED)
    {
        return 0;
    }
    LLM_Status = LLM_STATUS_IDLE;
    return 1;
}

uint8_t LLM_SendPrompt(char *Prompt, char *Response)
{
    char HttpHeader[512];
    char HttpBody[1024];
    uint16_t BodyLen;
    uint32_t Timer = 0;
    char *pStart, *pEnd;

    LLM_Status = LLM_STATUS_CONNECTING;

    if(WIFI_ConnectTCP(LLM_HOST, LLM_PORT) != 1)
    {
        LLM_Status = LLM_STATUS_ERROR;
        return 0;
    }

    LLM_Status = LLM_STATUS_SENDING;

    sprintf(HttpBody, "{\"model\":\"%s\",\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}]}", 
            LLM_MODEL, Prompt);
    BodyLen = strlen(HttpBody);

    sprintf(HttpHeader, 
            "POST /v1/chat/completions HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Content-Type: application/json\r\n"
            "Authorization: Bearer %s\r\n"
            "Content-Length: %d\r\n"
            "\r\n",
            LLM_HOST, LLM_API_KEY, BodyLen);

    USART1_SendString(HttpHeader);
    USART1_SendString(HttpBody);

    LLM_Status = LLM_STATUS_RECEIVING;

    Timer = 0;
    USART1_RxFlag = 0;
    USART1_RxLen = 0;

    while(Timer < LLM_TIMEOUT)
    {
        if(USART1_RxFlag == 1)
        {
            if(strstr((char*)USART1_RxBuffer, "\"content\":") != NULL)
            {
                pStart = strstr((char*)USART1_RxBuffer, "\"content\":\"") + 11;
                pEnd = strstr(pStart, "\"");
                if(pStart && pEnd)
                {
                    strncpy(Response, pStart, pEnd - pStart);
                    Response[pEnd - pStart] = '\0';
                    WIFI_CloseTCP();
                    LLM_Status = LLM_STATUS_COMPLETED;
                    return 1;
                }
            }
            USART1_RxFlag = 0;
        }
        vTaskDelay(10);
        Timer += 10;
    }

    WIFI_CloseTCP();
    LLM_Status = LLM_STATUS_ERROR;
    return 0;
}

uint8_t LLM_AnalyzeEnvironment(float Temp, float Hum, uint16_t FlameValue, uint8_t ShockFlag, char *Analysis)
{
    char Prompt[LLM_MAX_PROMPT];
    char FlameStatus[16];
    char ShockStatus[16];

    if(FlameValue < 300)
        strcpy(FlameStatus, "Flame detected");
    else
        strcpy(FlameStatus, "No flame");

    if(ShockFlag == 1)
        strcpy(ShockStatus, "Shock detected");
    else
        strcpy(ShockStatus, "No shock");

    sprintf(Prompt, 
            "Environment analysis: Temp=%.1fC, Hum=%d%%, Flame=%s, Shock=%s. Give brief Chinese assessment and safety advice (under 30 chars).",
            Temp, (uint8_t)Hum, FlameStatus, ShockStatus);

    return LLM_SendPrompt(Prompt, Analysis);
}
