#ifndef __CMDPARSER_H
#define __CMDPARSER_H

#include "stm32f10x.h"

typedef enum {
	CMD_NONE = 0,
	CMD_RELAY_ON,
	CMD_RELAY_OFF,
	CMD_BUZZER_ON,
	CMD_BUZZER_OFF,
	CMD_LED_ON,
	CMD_LED_OFF,
	CMD_RGB_RED,
	CMD_RGB_GREEN,
	CMD_RGB_BLUE,
	CMD_RGB_OFF,
	CMD_GET_STATUS,
	CMD_SET_THRESHOLD,
	CMD_AI_ANALYZE,
	CMD_AI_RESULT,
	CMD_ONENET_SET
} CmdTypeDef;

typedef struct {
	CmdTypeDef Type;
	int32_t Param1;
	int32_t Param2;
	char Extra[64];
	char MsgId[48];
	uint8_t IsOneNetSet;
	uint8_t IsOneNetService;
	char ReplyTopic[128];
	uint8_t OneNetMask;
	uint8_t OneNetRelay;
	uint8_t OneNetBuzzer;
	uint8_t OneNetLed;
	uint8_t OneNetRgb;
} CmdStruct;

extern CmdStruct CurrentCmd;

void CmdParser_Init(void);
CmdTypeDef CmdParser_Parse(char *Payload);
void CmdParser_Execute(CmdTypeDef Cmd);
void CmdParser_SendResponse(char *Msg);

#endif
