#ifndef __MQTT_H
#define __MQTT_H

#include "stm32f10x.h"
#include "MQTT_Config.h"

/*
 * MQTT.h 只保留协议参数和函数声明。
 * WiFi、OneNET 产品 ID、设备名、Token 和 Topic 等私有配置放在 MQTT_Config.h。
 * 公开仓库只提交 MQTT_Config_Template.h，不提交真实 MQTT_Config.h。
 */

/* MQTT 参数配置 */
#define MQTT_MAX_PAYLOAD   256  /* 最大负载长度 */
#define MQTT_MAX_TOPIC     128  /* 最大主题长度 */
#define MQTT_MAX_PACKET    512  /* 最大发布报文长度 */
#define MQTT_TIMEOUT       5000 /* 连接超时时间，单位 ms */

/**
 * @brief MQTT 连接状态枚举
 */
typedef enum {
    MQTT_STATUS_IDLE = 0,        /* 空闲状态 */
    MQTT_STATUS_CONNECTING,      /* 正在连接 */
    MQTT_STATUS_CONNECTED,       /* 已连接 */
    MQTT_STATUS_SUBSCRIBED,      /* 已订阅 */
    MQTT_STATUS_ERROR            /* 连接错误 */
} MQTT_StatusTypeDef;

/* MQTT 全局状态变量 */
extern MQTT_StatusTypeDef MQTT_Status;       /* 当前 MQTT 连接状态 */
extern char MQTT_RxPayload[MQTT_MAX_PAYLOAD];/* 接收负载缓冲区 */
extern char MQTT_RxTopic[MQTT_MAX_TOPIC];    /* 接收主题缓冲区 */
extern uint8_t MQTT_RxFlag;                  /* 接收完成标志 */
extern uint8_t MQTT_DiagRxCount;             /* 接收计数，调试用 */
extern uint8_t MQTT_ConnectSubStep;          /* 连接子步骤，调试用 */

uint8_t MQTT_Init(void);
uint8_t MQTT_ClientInit(void);
uint8_t MQTT_Connect(void);
uint8_t MQTT_Subscribe(char *Topic);
uint8_t MQTT_Publish(char *Topic, char *Payload);
uint8_t MQTT_Ping(void);
uint8_t MQTT_ProcessRx(void);
void MQTT_ClearRxBuffer(void);

#endif /* __MQTT_H */