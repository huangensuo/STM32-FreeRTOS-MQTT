#include "WiFi.h"
#include "USART.h"
#include "../System/Delay.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

WIFI_StatusTypeDef WIFI_Status = WIFI_STATUS_IDLE;

uint8_t WIFI_Init(void)
{
    uint8_t Retry;

    USART1_Init(115200);
    vTaskDelay(100);

    for(Retry = 0; Retry < WIFI_MAX_RETRY; Retry++)
    {
        USART1_ClearRxBuffer();
        USART1_SendString("AT\r\n");
        if(USART1_WaitForResponse("OK", 2000) == 1)
        {
            WIFI_Status = WIFI_STATUS_CONNECTED;
            return 1;
        }
        vTaskDelay(500);
    }
    WIFI_Status = WIFI_STATUS_ERROR;
    return 0;
}

uint8_t WIFI_SetMode(uint8_t Mode)
{
    char Cmd[32];

    USART1_ClearRxBuffer();
    sprintf(Cmd, "AT+CWMODE=%d\r\n", Mode);
    USART1_SendString(Cmd);
    if(USART1_WaitForResponse("OK", 2000) == 1)
    {
        vTaskDelay(500);
        return 1;
    }
    return 0;
}

uint8_t WIFI_Connect(char *SSID, char *Password)
{
    char Cmd[128];
    uint8_t Retry;

    WIFI_SetMode(1);

    for(Retry = 0; Retry < WIFI_MAX_RETRY; Retry++)
    {
        USART1_ClearRxBuffer();
        sprintf(Cmd, "AT+CWJAP=\"%s\",\"%s\"\r\n", SSID, Password);
        USART1_SendString(Cmd);
        if(USART1_WaitForResponse("OK", WIFI_TIMEOUT) == 1)
        {
            WIFI_Status = WIFI_STATUS_CONNECTED;
            return 1;
        }
        vTaskDelay(1000);
    }
    WIFI_Status = WIFI_STATUS_ERROR;
    return 0;
}

uint8_t WIFI_Disconnect(void)
{
    USART1_ClearRxBuffer();
    USART1_SendString("AT+CWQAP\r\n");
    if(USART1_WaitForResponse("OK", 2000) == 1)
    {
        WIFI_Status = WIFI_STATUS_DISCONNECTED;
        return 1;
    }
    return 0;
}

uint8_t WIFI_GetIP(char *IP)
{
    char *pStart, *pEnd;
    uint16_t Len;

    USART1_ClearRxBuffer();
    USART1_SendString("AT+CIFSR\r\n");
    if(USART1_WaitForResponse("CIFSR:STAIP", 2000) == 1)
    {
        pStart = strstr((char*)USART1_RxBuffer, "\"") + 1;
        pEnd = strstr(pStart, "\"");
        if(pStart && pEnd)
        {
            Len = pEnd - pStart;
            strncpy(IP, pStart, Len);
            IP[Len] = '\0';
            return 1;
        }
    }
    return 0;
}

uint8_t WIFI_EnableMUX(uint8_t Mode)
{
    char Cmd[32];

    USART1_ClearRxBuffer();
    sprintf(Cmd, "AT+CIPMUX=%d\r\n", Mode);
    USART1_SendString(Cmd);
    if(USART1_WaitForResponse("OK", 2000) == 1)
    {
        return 1;
    }
    return 0;
}

uint8_t WIFI_ConnectTCP(char *Host, uint16_t Port)
{
    char Cmd[128];

    WIFI_EnableMUX(0);

    USART1_ClearRxBuffer();
    sprintf(Cmd, "AT+CIPSTART=\"TCP\",\"%s\",%d\r\n", Host, Port);
    USART1_SendString(Cmd);
    if(USART1_WaitForResponse("CONNECT", 3000) == 1)
    {
        return 1;
    }
    return 0;
}

uint8_t WIFI_SendTCPData(uint8_t *Data, uint16_t Length)
{
    char Cmd[64];

    USART1_ClearRxBuffer();
    sprintf(Cmd, "AT+CIPSEND=%d\r\n", Length);
    USART1_SendString(Cmd);

    if(USART1_WaitForResponse(">", 1000) == 1)
    {
        vTaskDelay(50);
        USART1_SendData(Data, Length);
        if(USART1_WaitForResponse("SEND OK", 3000) == 1)
        {
            return 1;
        }
    }
    return 0;
}

uint8_t WIFI_CloseTCP(void)
{
    USART1_ClearRxBuffer();
    USART1_SendString("AT+CIPCLOSE\r\n");
    if(USART1_WaitForResponse("OK", 2000) == 1)
    {
        return 1;
    }
    return 0;
}
