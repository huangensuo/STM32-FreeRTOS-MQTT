#ifndef __WIFI_H
#define __WIFI_H

#include "stm32f10x.h"

#define WIFI_MAX_RETRY  5
#define WIFI_TIMEOUT    5000

typedef enum {
    WIFI_STATUS_IDLE = 0,
    WIFI_STATUS_CONNECTED,
    WIFI_STATUS_DISCONNECTED,
    WIFI_STATUS_ERROR
} WIFI_StatusTypeDef;

extern WIFI_StatusTypeDef WIFI_Status;

uint8_t WIFI_Init(void);
uint8_t WIFI_Connect(char *SSID, char *Password);
uint8_t WIFI_Disconnect(void);
uint8_t WIFI_SetMode(uint8_t Mode);
uint8_t WIFI_GetIP(char *IP);
uint8_t WIFI_EnableMUX(uint8_t Mode);
uint8_t WIFI_ConnectTCP(char *Host, uint16_t Port);
uint8_t WIFI_SendTCPData(uint8_t *Data, uint16_t Length);
uint8_t WIFI_CloseTCP(void);

#endif
