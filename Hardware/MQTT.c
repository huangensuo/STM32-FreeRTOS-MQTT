#include "MQTT.h"
#include <string.h>
#include <stdio.h>
#include "USART.h"
#include "../System/Delay.h"
#include "FreeRTOS.h"
#include "task.h"

extern uint8_t MQTT_ConnectState;
uint8_t MQTT_DiagRxCount = 0;
uint8_t MQTT_ConnectSubStep = 0;

MQTT_StatusTypeDef MQTT_Status = MQTT_STATUS_IDLE;
char MQTT_RxPayload[MQTT_MAX_PAYLOAD];
char MQTT_RxTopic[MQTT_MAX_TOPIC];
uint8_t MQTT_RxFlag = 0;

static void MQTT_SubscribeControlTopics(void)
{
    MQTT_Subscribe(MQTT_TOPIC_CMD);
    vTaskDelay(100);
    MQTT_Subscribe(MQTT_TOPIC_POST_REPLY);
    vTaskDelay(100);
    MQTT_Subscribe(MQTT_TOPIC_RELAY_INVOKE);
    vTaskDelay(100);
    MQTT_Subscribe(MQTT_TOPIC_BUZZER_INVOKE);
    vTaskDelay(100);
    MQTT_Subscribe(MQTT_TOPIC_LED_INVOKE);
    vTaskDelay(100);
    MQTT_Subscribe(MQTT_TOPIC_RGB_INVOKE);
    vTaskDelay(100);
    MQTT_Subscribe(MQTT_TOPIC_STATUS_INVOKE);
    vTaskDelay(100);
    MQTT_Subscribe(MQTT_TOPIC_AI_INVOKE);
    vTaskDelay(100);
    MQTT_Subscribe(MQTT_TOPIC_TEMP_INVOKE);
    vTaskDelay(100);
    MQTT_Subscribe(MQTT_TOPIC_HUM_INVOKE);
}

uint8_t MQTT_Init(void)
{
    USART1_Init(115200);
    vTaskDelay(500);
    MQTT_Status = MQTT_STATUS_IDLE;
    return 1;
}

uint8_t MQTT_ClientInit(void)
{
    MQTT_Status = MQTT_STATUS_IDLE;
    return 1;
}

uint8_t MQTT_Connect(void)
{
    char Cmd[256];
    uint8_t Retry;

    MQTT_Status = MQTT_STATUS_CONNECTING;
    MQTT_ConnectSubStep = 0;

    USART1_ClearRxBuffer();
    sprintf(Cmd, "AT+CIPSTART=\"TCP\",\"%s\",%d\r\n", MQTT_BROKER, MQTT_PORT);
    USART1_SendString(Cmd);
    if(USART1_WaitForResponse("OK", 10000) != 1)
    {
        return 0;
    }
    vTaskDelay(500);

    MQTT_ConnectSubStep = 1;

    USART1_SendString("ATE0\r\n");
    USART1_WaitForResponse("OK", 1000);
    vTaskDelay(100);
    MQTT_ConnectSubStep = 2;

    uint8_t ConnectPacket[384];
    uint16_t PacketLen = 0;

    PacketLen = 0;
    ConnectPacket[PacketLen++] = 0x10;

    uint16_t RemainingLen = 0;
    uint8_t ProtocolNameLen = 4;
    uint8_t ProtocolLevel = 4;
    uint8_t ConnectFlags = 0x02;
    uint16_t KeepAlive = 60;

    uint16_t ClientIdLen = strlen(MQTT_CLIENT_ID);
    uint16_t UsernameLen = strlen(MQTT_USERNAME);
    uint16_t PasswordLen = strlen(MQTT_PASSWORD);

    RemainingLen = 10 + 2 + ClientIdLen;

    if(MQTT_USERNAME[0] != '\0')
    {
        ConnectFlags |= 0x80;
        RemainingLen += 2 + UsernameLen;
    }
    if(MQTT_PASSWORD[0] != '\0')
    {
        ConnectFlags |= 0x40;
        RemainingLen += 2 + PasswordLen;
    }

    if(RemainingLen > 127)
    {
        ConnectPacket[PacketLen++] = 0x80 | (RemainingLen & 0x7F);
        ConnectPacket[PacketLen++] = (RemainingLen >> 7) & 0x7F;
    }
    else
    {
        ConnectPacket[PacketLen++] = RemainingLen;
    }

    ConnectPacket[PacketLen++] = 0x00;
    ConnectPacket[PacketLen++] = ProtocolNameLen;
    memcpy(ConnectPacket + PacketLen, "MQTT", ProtocolNameLen);
    PacketLen += ProtocolNameLen;

    ConnectPacket[PacketLen++] = ProtocolLevel;
    ConnectPacket[PacketLen++] = ConnectFlags;

    ConnectPacket[PacketLen++] = (KeepAlive >> 8) & 0xFF;
    ConnectPacket[PacketLen++] = KeepAlive & 0xFF;

    ConnectPacket[PacketLen++] = (ClientIdLen >> 8) & 0xFF;
    ConnectPacket[PacketLen++] = ClientIdLen & 0xFF;
    memcpy(ConnectPacket + PacketLen, MQTT_CLIENT_ID, ClientIdLen);
    PacketLen += ClientIdLen;

    if(MQTT_USERNAME[0] != '\0')
    {
        ConnectPacket[PacketLen++] = (UsernameLen >> 8) & 0xFF;
        ConnectPacket[PacketLen++] = UsernameLen & 0xFF;
        memcpy(ConnectPacket + PacketLen, MQTT_USERNAME, UsernameLen);
        PacketLen += UsernameLen;
    }
    if(MQTT_PASSWORD[0] != '\0')
    {
        ConnectPacket[PacketLen++] = (PasswordLen >> 8) & 0xFF;
        ConnectPacket[PacketLen++] = PasswordLen & 0xFF;
        memcpy(ConnectPacket + PacketLen, MQTT_PASSWORD, PasswordLen);
        PacketLen += PasswordLen;
    }

    for(Retry = 0; Retry < 3; Retry++)
    {
        MQTT_ConnectSubStep = 3;

        USART1_ClearRxBuffer();
        sprintf(Cmd, "AT+CIPSEND=%d\r\n", PacketLen);
        USART1_SendString(Cmd);
        if(USART1_WaitForResponse(">", 2000) != 1) { vTaskDelay(1000); continue; }

        MQTT_ConnectSubStep = 4;

        vTaskDelay(50);
        USART1_SendData(ConnectPacket, PacketLen);

        MQTT_ConnectSubStep = 5;

        if(USART1_WaitForResponse("SEND OK", 3000) != 1) { vTaskDelay(1000); continue; }

        MQTT_ConnectSubStep = 6;

        {
            uint32_t Timer = 0;
            USART1_RxFlag = 0;
            while(Timer < 5000)
            {
                if(USART1_RxFlag == 1)
                {
                    USART1_RxBuffer[USART1_RxLen] = '\0';
                    if(strstr((char*)USART1_RxBuffer, "+IPD") != NULL)
                    {
                        USART1_ClearRxBuffer();
                        MQTT_Status = MQTT_STATUS_CONNECTED;

                        vTaskDelay(200);
                        MQTT_SubscribeControlTopics();

                        return 1;
                    }
                    USART1_RxFlag = 0;
                }
                vTaskDelay(1); Timer++;
            }
        }
        vTaskDelay(1000);
    }
    MQTT_Status = MQTT_STATUS_ERROR;
    return 0;
}

uint8_t MQTT_Subscribe(char *Topic)
{
    uint8_t SubscribePacket[256];
    uint16_t PacketLen = 0;
    uint16_t TopicLen = strlen(Topic);
    static uint16_t PacketId = 1;
    char Cmd[64];

    PacketLen = 0;
    SubscribePacket[PacketLen++] = 0x82;

    uint16_t RemainingLen = 2 + 2 + TopicLen + 1;

    if(RemainingLen > 127)
    {
        SubscribePacket[PacketLen++] = 0x80 | (RemainingLen & 0x7F);
        SubscribePacket[PacketLen++] = (RemainingLen >> 7) & 0x7F;
    }
    else
    {
        SubscribePacket[PacketLen++] = RemainingLen;
    }

    SubscribePacket[PacketLen++] = (PacketId >> 8) & 0xFF;
    SubscribePacket[PacketLen++] = PacketId & 0xFF;

    SubscribePacket[PacketLen++] = (TopicLen >> 8) & 0xFF;
    SubscribePacket[PacketLen++] = TopicLen & 0xFF;
    memcpy(SubscribePacket + PacketLen, Topic, TopicLen);
    PacketLen += TopicLen;

    SubscribePacket[PacketLen++] = 0x00;

    PacketId++;

    USART1_SaveIPD();

    sprintf(Cmd, "AT+CIPSEND=%d\r\n", PacketLen);
    USART1_SendString(Cmd);
    if(USART1_WaitForResponse(">", 1000) == 1)
    {
        vTaskDelay(50);
        USART1_SendData(SubscribePacket, PacketLen);
        if(USART1_WaitForResponse("SEND OK", 3000) == 1)
        {
            vTaskDelay(300);
            USART1_SaveIPD();
            MQTT_Status = MQTT_STATUS_SUBSCRIBED;
            return 1;
        }
    }
    return 0;
}

uint8_t MQTT_Publish(char *Topic, char *Payload)
{
    uint8_t PublishPacket[MQTT_MAX_PACKET];
    uint16_t PacketLen = 0;
    uint16_t TopicLen = strlen(Topic);
    uint16_t PayloadLen = strlen(Payload);
    char Cmd[64];
    uint16_t RemainingLen = 2 + TopicLen + PayloadLen;

    if(RemainingLen + 4 > MQTT_MAX_PACKET)
    {
        return 0;
    }

    PacketLen = 0;
    PublishPacket[PacketLen++] = 0x30;

    if(RemainingLen > 127)
    {
        PublishPacket[PacketLen++] = 0x80 | (RemainingLen & 0x7F);
        PublishPacket[PacketLen++] = (RemainingLen >> 7) & 0x7F;
    }
    else
    {
        PublishPacket[PacketLen++] = RemainingLen;
    }

    PublishPacket[PacketLen++] = (TopicLen >> 8) & 0xFF;
    PublishPacket[PacketLen++] = TopicLen & 0xFF;
    memcpy(PublishPacket + PacketLen, Topic, TopicLen);
    PacketLen += TopicLen;

    memcpy(PublishPacket + PacketLen, Payload, PayloadLen);
    PacketLen += PayloadLen;

    USART1_SaveIPD();

    sprintf(Cmd, "AT+CIPSEND=%d\r\n", PacketLen);
    USART1_SendString(Cmd);
    if(USART1_WaitForResponse(">", 1000) == 1)
    {
        vTaskDelay(50);
        USART1_SendData(PublishPacket, PacketLen);
        if(USART1_WaitForResponse("SEND OK", 3000) == 1)
        {
            vTaskDelay(300);
            USART1_SaveIPD();
            return 1;
        }
    }
    return 0;
}

uint8_t MQTT_Ping(void)
{
    char Cmd[64];

    USART1_SaveIPD();

    sprintf(Cmd, "AT+CIPSEND=2\r\n");
    USART1_SendString(Cmd);
    if(USART1_WaitForResponse(">", 1000) == 1)
    {
        vTaskDelay(50);
        USART1_SendData((uint8_t*)"\xC0\x00", 2);
        if(USART1_WaitForResponse("SEND OK", 3000) == 1)
        {
            vTaskDelay(300);
            USART1_SaveIPD();
            return 1;
        }
    }
    return 0;
}

uint8_t MQTT_ProcessRx(void)
{
    uint8_t *data;
    uint16_t dataLen;
    uint16_t offset;
    uint8_t anyProcessed = 0;

    USART1_CheckIPD();

    if(!IPD_Ready) return 0;

    IPD_Ready = 0;
    MQTT_DiagRxCount++;

    data = IPD_Buffer;
    dataLen = IPD_Len;
    offset = 0;

    while(offset + 3 <= dataLen)
    {
        uint8_t type = data[offset] & 0xF0;
        if(type != 0x30) break;

        uint16_t idx = offset + 1;
        uint32_t remLen = 0;
        uint8_t mult = 0, b;
        do {
            if(idx >= dataLen) return anyProcessed;
            b = data[idx++];
            remLen |= (uint32_t)(b & 0x7F) << (mult * 7);
            mult++;
        } while(b & 0x80 && mult < 4);

        uint16_t pktEnd = idx + (uint16_t)remLen;
        if(pktEnd > dataLen) return anyProcessed;

        if(idx + 2 > dataLen) return anyProcessed;
        uint16_t topicLen = ((uint16_t)data[idx] << 8) | data[idx+1];
        idx += 2;
        if(idx + topicLen > dataLen) return anyProcessed;
        {
            uint16_t copyLen = (topicLen < MQTT_MAX_TOPIC - 1) ? topicLen : (MQTT_MAX_TOPIC - 1);
            memcpy(MQTT_RxTopic, data + idx, copyLen);
            MQTT_RxTopic[copyLen] = '\0';
        }
        idx += topicLen;

        uint8_t qos = (data[offset] & 0x06) >> 1;
        if(qos > 0) idx += 2;
        if(idx >= pktEnd) { offset = pktEnd; continue; }

        uint16_t payloadLen = pktEnd - idx;
        if(payloadLen < MQTT_MAX_PAYLOAD)
        {
            memcpy(MQTT_RxPayload, data + idx, payloadLen);
            MQTT_RxPayload[payloadLen] = '\0';
            MQTT_RxFlag = 1;
            anyProcessed = 1;
        }

        offset = pktEnd;
    }

    return anyProcessed;
}

void MQTT_ClearRxBuffer(void)
{
    uint16_t i;
    for(i = 0; i < MQTT_MAX_PAYLOAD; i++)
    {
        MQTT_RxPayload[i] = 0;
    }
    for(i = 0; i < MQTT_MAX_TOPIC; i++)
    {
        MQTT_RxTopic[i] = 0;
    }
    MQTT_RxFlag = 0;
}
