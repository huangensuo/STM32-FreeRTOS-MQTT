#include "CmdParser.h"
#include "Relay.h"
#include "Buzzer.h"
#include "LED.h"
#include "RGB.h"
#include "MQTT.h"
#include "../User/AppConfig.h"
#include "task.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

CmdStruct CurrentCmd;

void CmdParser_Init(void)
{
	CurrentCmd.Type = CMD_NONE;
	CurrentCmd.Param1 = 0;
	CurrentCmd.Param2 = 0;
	memset(CurrentCmd.Extra, 0, sizeof(CurrentCmd.Extra));
	memset(CurrentCmd.MsgId, 0, sizeof(CurrentCmd.MsgId));
	strcpy(CurrentCmd.MsgId, "1");
	CurrentCmd.IsOneNetSet = 0;
	CurrentCmd.IsOneNetService = 0;
	memset(CurrentCmd.ReplyTopic, 0, sizeof(CurrentCmd.ReplyTopic));
	CurrentCmd.OneNetMask = 0;
	CurrentCmd.OneNetRelay = 0;
	CurrentCmd.OneNetBuzzer = 0;
	CurrentCmd.OneNetLed = 0;
	CurrentCmd.OneNetRgb = MANUAL_RGB_OFF;
}

static void CmdParser_ExtractOneNetId(char *Payload)
{
	char *p;
	uint8_t i;

	strcpy(CurrentCmd.MsgId, "1");
	p = strstr(Payload, "\"id\"");
	if(p == NULL) return;

	p = strchr(p, ':');
	if(p == NULL) return;

	p++;
	while(*p == ' ' || *p == '\"') p++;

	i = 0;
	while(*p && *p != '\"' && *p != ',' && *p != '}' && i < sizeof(CurrentCmd.MsgId) - 1)
	{
		CurrentCmd.MsgId[i++] = *p++;
	}
	CurrentCmd.MsgId[i] = '\0';
}

static uint8_t CmdParser_GetJsonInt(char *Payload, char *Name, int32_t *Value)
{
	char Key[24];
	char *p;
	char *objEnd;

	sprintf(Key, "\"%s\"", Name);
	p = strstr(Payload, Key);
	if(p == NULL) return 0;

	p = strchr(p, ':');
	if(p == NULL) return 0;
	p++;
	while(*p == ' ' || *p == '\t') p++;

	if(*p == '{')
	{
		objEnd = strchr(p, '}');
		p = strstr(p, "\"value\"");
		if(p == NULL) return 0;
		if(objEnd != NULL && p > objEnd) return 0;
		p = strchr(p, ':');
		if(p == NULL) return 0;
		p++;
		while(*p == ' ' || *p == '\t') p++;
	}

	if(*p == '\"') p++;
	*Value = atoi(p);
	return 1;
}

static CmdTypeDef CmdParser_ParseOneNetSet(char *Payload)
{
	int32_t value;
	uint8_t mask = 0;

	if(strstr(Payload, "\"params\"") == NULL)
		return CMD_NONE;

	if(CmdParser_GetJsonInt(Payload, "relay", &value) == 1)
	{
		mask |= MANUAL_OVERRIDE_RELAY_MASK;
		CurrentCmd.OneNetRelay = (value != 0) ? 1 : 0;
	}

	if(CmdParser_GetJsonInt(Payload, "buzzer", &value) == 1)
	{
		mask |= MANUAL_OVERRIDE_BUZZER_MASK;
		CurrentCmd.OneNetBuzzer = (value != 0) ? 1 : 0;
	}

	if(CmdParser_GetJsonInt(Payload, "led", &value) == 1)
	{
		mask |= MANUAL_OVERRIDE_LED_MASK;
		CurrentCmd.OneNetLed = (value != 0) ? 1 : 0;
	}

	if(CmdParser_GetJsonInt(Payload, "rgb", &value) == 1)
	{
		mask |= MANUAL_OVERRIDE_RGB_MASK;
		if(value == MANUAL_RGB_RED) CurrentCmd.OneNetRgb = MANUAL_RGB_RED;
		else if(value == MANUAL_RGB_GREEN) CurrentCmd.OneNetRgb = MANUAL_RGB_GREEN;
		else if(value == MANUAL_RGB_BLUE) CurrentCmd.OneNetRgb = MANUAL_RGB_BLUE;
		else CurrentCmd.OneNetRgb = MANUAL_RGB_OFF;
	}

	if(mask != 0)
	{
		CmdParser_ExtractOneNetId(Payload);
		CurrentCmd.IsOneNetSet = 1;
		CurrentCmd.OneNetMask = mask;
		return CMD_ONENET_SET;
	}

	return CMD_NONE;
}

static uint8_t CmdParser_GetServiceId(char *Topic, char *OutId, uint8_t OutSize)
{
	char *p;
	char *end;
	uint8_t i = 0;

	p = strstr(Topic, "/thing/service/");
	if(p == NULL) return 0;
	p += strlen("/thing/service/");

	end = strstr(p, "/invoke");
	if(end == NULL) return 0;

	while(p < end && i < OutSize - 1)
	{
		OutId[i++] = *p++;
	}
	OutId[i] = '\0';
	return (i > 0) ? 1 : 0;
}

static void CmdParser_BuildServiceReplyTopic(char *ServiceId)
{
	sprintf(CurrentCmd.ReplyTopic, "%s%s/invoke_reply", MQTT_TOPIC_SERVICE_PREFIX, ServiceId);
}

static CmdTypeDef CmdParser_ParseOneNetService(char *Payload)
{
	char serviceId[24];
	int32_t value = 0;

	if(CmdParser_GetServiceId(MQTT_RxTopic, serviceId, sizeof(serviceId)) == 0)
		return CMD_NONE;

	CmdParser_ExtractOneNetId(Payload);
	CmdParser_BuildServiceReplyTopic(serviceId);
	CurrentCmd.IsOneNetService = 1;

	if(strcmp(serviceId, "relay") == 0)
	{
		if(CmdParser_GetJsonInt(Payload, "value", &value) == 0)
			CmdParser_GetJsonInt(Payload, "relay", &value);
		CurrentCmd.OneNetMask = MANUAL_OVERRIDE_RELAY_MASK;
		CurrentCmd.OneNetRelay = (value != 0) ? 1 : 0;
		return CMD_ONENET_SET;
	}

	if(strcmp(serviceId, "buzzer") == 0)
	{
		if(CmdParser_GetJsonInt(Payload, "value", &value) == 0)
			CmdParser_GetJsonInt(Payload, "buzzer", &value);
		CurrentCmd.OneNetMask = MANUAL_OVERRIDE_BUZZER_MASK;
		CurrentCmd.OneNetBuzzer = (value != 0) ? 1 : 0;
		return CMD_ONENET_SET;
	}

	if(strcmp(serviceId, "led") == 0)
	{
		if(CmdParser_GetJsonInt(Payload, "value", &value) == 0)
			CmdParser_GetJsonInt(Payload, "led", &value);
		CurrentCmd.OneNetMask = MANUAL_OVERRIDE_LED_MASK;
		CurrentCmd.OneNetLed = (value != 0) ? 1 : 0;
		return CMD_ONENET_SET;
	}

	if(strcmp(serviceId, "rgb") == 0)
	{
		if(CmdParser_GetJsonInt(Payload, "value", &value) == 0)
			CmdParser_GetJsonInt(Payload, "rgb", &value);
		CurrentCmd.OneNetMask = MANUAL_OVERRIDE_RGB_MASK;
		if(value == MANUAL_RGB_RED) CurrentCmd.OneNetRgb = MANUAL_RGB_RED;
		else if(value == MANUAL_RGB_GREEN) CurrentCmd.OneNetRgb = MANUAL_RGB_GREEN;
		else if(value == MANUAL_RGB_BLUE) CurrentCmd.OneNetRgb = MANUAL_RGB_BLUE;
		else CurrentCmd.OneNetRgb = MANUAL_RGB_OFF;
		return CMD_ONENET_SET;
	}

	if(strcmp(serviceId, "get_status") == 0)
		return CMD_GET_STATUS;

	if(strcmp(serviceId, "ai_analyze") == 0)
		return CMD_AI_ANALYZE;

	if(strcmp(serviceId, "temp_threshold") == 0)
	{
		if(CmdParser_GetJsonInt(Payload, "value", &value) == 1)
			CurrentCmd.Param1 = value;
		return CMD_SET_THRESHOLD;
	}

	if(strcmp(serviceId, "hum_threshold") == 0)
	{
		if(CmdParser_GetJsonInt(Payload, "value", &value) == 1)
			CurrentCmd.Param2 = value;
		return CMD_SET_THRESHOLD;
	}

	return CMD_NONE;
}

static char *CmdParser_GetCmdName(CmdTypeDef Cmd)
{
	switch(Cmd)
	{
		case CMD_RELAY_ON: return "relay_on";
		case CMD_RELAY_OFF: return "relay_off";
		case CMD_BUZZER_ON: return "buzzer_on";
		case CMD_BUZZER_OFF: return "buzzer_off";
		case CMD_LED_ON: return "led_on";
		case CMD_LED_OFF: return "led_off";
		case CMD_RGB_RED: return "rgb_red";
		case CMD_RGB_GREEN: return "rgb_green";
		case CMD_RGB_BLUE: return "rgb_blue";
		case CMD_RGB_OFF: return "rgb_off";
		case CMD_GET_STATUS: return "get_status";
		case CMD_SET_THRESHOLD: return "set_threshold";
		case CMD_AI_ANALYZE: return "ai_analyze";
		case CMD_AI_RESULT: return "ai_result";
		case CMD_ONENET_SET: return "onenet_set";
		default: return "unknown";
	}
}

static uint8_t CmdParser_PublishWithRetry(char *Topic, char *Payload)
{
	uint8_t retry;

	for(retry = 0; retry < 3; retry++)
	{
		if(MQTT_Publish(Topic, Payload) == 1)
			return 1;
		vTaskDelay(100);
	}

	return 0;
}

static void CmdParser_PublishResult(CmdTypeDef Cmd, uint16_t Code, char *Msg)
{
	char Response[160];
	char *Topic;

	if(CurrentCmd.IsOneNetService == 1)
	{
		sprintf(Response, "{\"id\":\"%s\",\"code\":%d,\"msg\":\"%s\",\"data\":{}}",
				CurrentCmd.MsgId, Code, Msg);
		Topic = CurrentCmd.ReplyTopic;
	}
	else if(CurrentCmd.IsOneNetSet == 1)
	{
		sprintf(Response, "{\"id\":\"%s\",\"code\":%d,\"msg\":\"%s\"}",
				CurrentCmd.MsgId, Code, Msg);
		Topic = MQTT_TOPIC_SET_REPLY;
	}
	else
	{
		if(Code == 200)
			sprintf(Response, "{\"result\":\"ok\",\"cmd\":\"%s\"}", CmdParser_GetCmdName(Cmd));
		else
			sprintf(Response, "{\"result\":\"error\",\"cmd\":\"%s\",\"msg\":\"%s\"}", CmdParser_GetCmdName(Cmd), Msg);
		Topic = MQTT_TOPIC_RESP;
	}

	CmdParser_PublishWithRetry(Topic, Response);
}

static uint8_t CmdParser_IsManualCmd(CmdTypeDef Cmd)
{
	return (Cmd == CMD_RELAY_ON || Cmd == CMD_RELAY_OFF ||
			Cmd == CMD_BUZZER_ON || Cmd == CMD_BUZZER_OFF ||
			Cmd == CMD_LED_ON || Cmd == CMD_LED_OFF ||
			Cmd == CMD_RGB_RED || Cmd == CMD_RGB_GREEN ||
			Cmd == CMD_RGB_BLUE || Cmd == CMD_RGB_OFF ||
			Cmd == CMD_ONENET_SET);
}

static uint8_t CmdParser_IsOneNetSetBlockedByDanger(void)
{
	if((CurrentCmd.OneNetMask & MANUAL_OVERRIDE_RELAY_MASK) && CurrentCmd.OneNetRelay == 0)
		return 1;
	if((CurrentCmd.OneNetMask & MANUAL_OVERRIDE_BUZZER_MASK) && CurrentCmd.OneNetBuzzer == 0)
		return 1;
	if((CurrentCmd.OneNetMask & MANUAL_OVERRIDE_LED_MASK) && CurrentCmd.OneNetLed == 0)
		return 1;
	if((CurrentCmd.OneNetMask & MANUAL_OVERRIDE_RGB_MASK) &&
	   CurrentCmd.OneNetRgb != MANUAL_RGB_RED)
		return 1;
	return 0;
}

static uint8_t CmdParser_IsBlockedByDanger(CmdTypeDef Cmd)
{
	DeviceStateTypeDef state;

	if(CmdParser_IsManualCmd(Cmd) == 0)
		return 0;

	state = STATE_NORMAL;
	if(xSemaphoreTake(xSemaphoreStatus, portMAX_DELAY) == pdTRUE)
	{
		state = g_DeviceStatus.State;
		xSemaphoreGive(xSemaphoreStatus);
	}

	if(state != STATE_DANGER)
		return 0;

	if(Cmd == CMD_ONENET_SET)
		return CmdParser_IsOneNetSetBlockedByDanger();

	if(Cmd == CMD_RELAY_OFF || Cmd == CMD_BUZZER_OFF ||
	   Cmd == CMD_LED_OFF || Cmd == CMD_RGB_GREEN ||
	   Cmd == CMD_RGB_BLUE || Cmd == CMD_RGB_OFF)
	{
		return 1;
	}

	return 0;
}

static void CmdParser_SetOneNetManualOverride(void)
{
	if(xSemaphoreTake(xSemaphoreStatus, portMAX_DELAY) == pdTRUE)
	{
		if(g_DeviceStatus.ManualOverrideMs == 0)
			g_DeviceStatus.ManualOverrideMask = 0;

		g_DeviceStatus.ManualOverrideMs = MANUAL_OVERRIDE_MS;

		if(CurrentCmd.OneNetMask & MANUAL_OVERRIDE_RELAY_MASK)
		{
			g_DeviceStatus.ManualOverrideMask |= MANUAL_OVERRIDE_RELAY_MASK;
			g_DeviceStatus.ManualRelay = CurrentCmd.OneNetRelay;
		}

		if(CurrentCmd.OneNetMask & MANUAL_OVERRIDE_BUZZER_MASK)
		{
			g_DeviceStatus.ManualOverrideMask |= MANUAL_OVERRIDE_BUZZER_MASK;
			g_DeviceStatus.ManualBuzzer = CurrentCmd.OneNetBuzzer;
		}

		if(CurrentCmd.OneNetMask & MANUAL_OVERRIDE_LED_MASK)
		{
			g_DeviceStatus.ManualOverrideMask |= MANUAL_OVERRIDE_LED_MASK;
			g_DeviceStatus.ManualLed = CurrentCmd.OneNetLed;
		}

		if(CurrentCmd.OneNetMask & MANUAL_OVERRIDE_RGB_MASK)
		{
			g_DeviceStatus.ManualOverrideMask |= MANUAL_OVERRIDE_RGB_MASK;
			g_DeviceStatus.ManualRgb = CurrentCmd.OneNetRgb;
		}

		xSemaphoreGive(xSemaphoreStatus);
	}
}

static void CmdParser_ExecuteOneNetSet(void)
{
	if(CurrentCmd.OneNetMask & MANUAL_OVERRIDE_RELAY_MASK)
	{
		if(CurrentCmd.OneNetRelay) Relay_Open();
		else Relay_Close();
	}

	if(CurrentCmd.OneNetMask & MANUAL_OVERRIDE_BUZZER_MASK)
	{
		if(CurrentCmd.OneNetBuzzer) Buzzer_On();
		else Buzzer_Off();
	}

	if(CurrentCmd.OneNetMask & MANUAL_OVERRIDE_LED_MASK)
	{
		if(CurrentCmd.OneNetLed) LED_On();
		else LED_Off();
	}

	if(CurrentCmd.OneNetMask & MANUAL_OVERRIDE_RGB_MASK)
	{
		if(CurrentCmd.OneNetRgb == MANUAL_RGB_RED) RGB_Red();
		else if(CurrentCmd.OneNetRgb == MANUAL_RGB_GREEN) RGB_Green();
		else if(CurrentCmd.OneNetRgb == MANUAL_RGB_BLUE) RGB_Blue();
		else RGB_Off();
	}

	CmdParser_SetOneNetManualOverride();
	CmdParser_PublishResult(CMD_ONENET_SET, 200, "success");
}

static void CmdParser_SetManualOverride(CmdTypeDef Cmd)
{
	if(CmdParser_IsManualCmd(Cmd) == 0)
		return;

	if(xSemaphoreTake(xSemaphoreStatus, portMAX_DELAY) == pdTRUE)
	{
		if(g_DeviceStatus.ManualOverrideMs == 0)
			g_DeviceStatus.ManualOverrideMask = 0;

		g_DeviceStatus.ManualOverrideMs = MANUAL_OVERRIDE_MS;

		switch(Cmd)
		{
			case CMD_RELAY_ON:
				g_DeviceStatus.ManualOverrideMask |= MANUAL_OVERRIDE_RELAY_MASK;
				g_DeviceStatus.ManualRelay = 1;
				break;

			case CMD_RELAY_OFF:
				g_DeviceStatus.ManualOverrideMask |= MANUAL_OVERRIDE_RELAY_MASK;
				g_DeviceStatus.ManualRelay = 0;
				break;

			case CMD_BUZZER_ON:
				g_DeviceStatus.ManualOverrideMask |= MANUAL_OVERRIDE_BUZZER_MASK;
				g_DeviceStatus.ManualBuzzer = 1;
				break;

			case CMD_BUZZER_OFF:
				g_DeviceStatus.ManualOverrideMask |= MANUAL_OVERRIDE_BUZZER_MASK;
				g_DeviceStatus.ManualBuzzer = 0;
				break;

			case CMD_LED_ON:
				g_DeviceStatus.ManualOverrideMask |= MANUAL_OVERRIDE_LED_MASK;
				g_DeviceStatus.ManualLed = 1;
				break;

			case CMD_LED_OFF:
				g_DeviceStatus.ManualOverrideMask |= MANUAL_OVERRIDE_LED_MASK;
				g_DeviceStatus.ManualLed = 0;
				break;

			case CMD_RGB_RED:
				g_DeviceStatus.ManualOverrideMask |= MANUAL_OVERRIDE_RGB_MASK;
				g_DeviceStatus.ManualRgb = MANUAL_RGB_RED;
				break;

			case CMD_RGB_GREEN:
				g_DeviceStatus.ManualOverrideMask |= MANUAL_OVERRIDE_RGB_MASK;
				g_DeviceStatus.ManualRgb = MANUAL_RGB_GREEN;
				break;

			case CMD_RGB_BLUE:
				g_DeviceStatus.ManualOverrideMask |= MANUAL_OVERRIDE_RGB_MASK;
				g_DeviceStatus.ManualRgb = MANUAL_RGB_BLUE;
				break;

			case CMD_RGB_OFF:
				g_DeviceStatus.ManualOverrideMask |= MANUAL_OVERRIDE_RGB_MASK;
				g_DeviceStatus.ManualRgb = MANUAL_RGB_OFF;
				break;

			default:
				break;
		}

		xSemaphoreGive(xSemaphoreStatus);
	}
}

CmdTypeDef CmdParser_Parse(char *Payload)
{
	CmdTypeDef oneNetServiceCmd;
	CmdTypeDef oneNetCmd;
	char *p;
	uint8_t i;

	CmdParser_Init();

	oneNetServiceCmd = CmdParser_ParseOneNetService(Payload);
	if(oneNetServiceCmd != CMD_NONE)
		return oneNetServiceCmd;

	oneNetCmd = CmdParser_ParseOneNetSet(Payload);
	if(oneNetCmd != CMD_NONE)
		return oneNetCmd;

	if(strncmp(Payload, "relay_on", 8) == 0)
		return CMD_RELAY_ON;
	if(strncmp(Payload, "relay_off", 9) == 0)
		return CMD_RELAY_OFF;
	if(strncmp(Payload, "buzzer_on", 9) == 0)
		return CMD_BUZZER_ON;
	if(strncmp(Payload, "buzzer_off", 10) == 0)
		return CMD_BUZZER_OFF;
	if(strncmp(Payload, "led_on", 6) == 0)
		return CMD_LED_ON;
	if(strncmp(Payload, "led_off", 7) == 0)
		return CMD_LED_OFF;
	if(strncmp(Payload, "rgb_red", 7) == 0)
		return CMD_RGB_RED;
	if(strncmp(Payload, "rgb_green", 9) == 0)
		return CMD_RGB_GREEN;
	if(strncmp(Payload, "rgb_blue", 8) == 0)
		return CMD_RGB_BLUE;
	if(strncmp(Payload, "rgb_off", 7) == 0)
		return CMD_RGB_OFF;
	if(strncmp(Payload, "get_status", 10) == 0)
		return CMD_GET_STATUS;
	if(strncmp(Payload, "ai_analyze", 10) == 0)
		return CMD_AI_ANALYZE;

	if(strncmp(Payload, "set_threshold", 13) == 0)
	{
		p = Payload + 14;
		CurrentCmd.Param1 = atoi(p);
		p = strchr(p, ',');
		if(p)
			CurrentCmd.Param2 = atoi(p + 1);
		return CMD_SET_THRESHOLD;
	}

	/* Check for AI analysis result (JSON format from server) */
	if(strstr(Payload, "\"type\":\"ai_analysis\"") != NULL ||
	   strstr(Payload, "\"type\": \"ai_analysis\"") != NULL)
	{
		/* Extract the "result" field value */
		p = strstr(Payload, "\"result\":");
		if(p != NULL)
		{
			p += 9;  /* Skip "result":" */
			if(*p == '"') p++;
			/* Copy to Extra buffer (max 63 chars) */
			i = 0;
			while(*p && *p != '"' && i < 63)
			{
				CurrentCmd.Extra[i++] = *p++;
			}
			CurrentCmd.Extra[i] = '\0';
		}
		return CMD_AI_RESULT;
	}

	return CMD_NONE;
}

void CmdParser_Execute(CmdTypeDef Cmd)
{
	char Response[192];

	if(CmdParser_IsBlockedByDanger(Cmd) == 1)
	{
		CmdParser_PublishResult(Cmd, 403, "danger_locked");
		return;
	}
	
	switch(Cmd)
	{
		case CMD_RELAY_ON:
			Relay_Open();
			CmdParser_SetManualOverride(Cmd);
			CmdParser_PublishResult(Cmd, 200, "success");
			break;

		case CMD_RELAY_OFF:
			Relay_Close();
			CmdParser_SetManualOverride(Cmd);
			CmdParser_PublishResult(Cmd, 200, "success");
			break;

		case CMD_BUZZER_ON:
			Buzzer_On();
			CmdParser_SetManualOverride(Cmd);
			CmdParser_PublishResult(Cmd, 200, "success");
			break;

		case CMD_BUZZER_OFF:
			Buzzer_Off();
			CmdParser_SetManualOverride(Cmd);
			CmdParser_PublishResult(Cmd, 200, "success");
			break;

		case CMD_LED_ON:
			LED_On();
			CmdParser_SetManualOverride(Cmd);
			CmdParser_PublishResult(Cmd, 200, "success");
			break;

		case CMD_LED_OFF:
			LED_Off();
			CmdParser_SetManualOverride(Cmd);
			CmdParser_PublishResult(Cmd, 200, "success");
			break;

		case CMD_RGB_RED:
			RGB_Red();
			CmdParser_SetManualOverride(Cmd);
			CmdParser_PublishResult(Cmd, 200, "success");
			break;

		case CMD_RGB_GREEN:
			RGB_Green();
			CmdParser_SetManualOverride(Cmd);
			CmdParser_PublishResult(Cmd, 200, "success");
			break;

		case CMD_RGB_BLUE:
			RGB_Blue();
			CmdParser_SetManualOverride(Cmd);
			CmdParser_PublishResult(Cmd, 200, "success");
			break;

		case CMD_RGB_OFF:
			RGB_Off();
			CmdParser_SetManualOverride(Cmd);
			CmdParser_PublishResult(Cmd, 200, "success");
			break;

		case CMD_GET_STATUS:
			if(CurrentCmd.IsOneNetService == 1)
			{
				sprintf(Response, "{\"id\":\"%s\",\"code\":200,\"msg\":\"success\",\"data\":{\"temp\":%.1f,\"hum\":%d,\"flame\":%d,\"shock\":%d,\"state\":%d}}",
						CurrentCmd.MsgId, g_SensorData.PrecisionTemp, g_SensorData.Humidity, g_SensorData.FlameFlag, g_SensorData.ShockFlag, (int)g_DeviceStatus.State);
				CmdParser_PublishWithRetry(CurrentCmd.ReplyTopic, Response);
			}
			else
			{
				sprintf(Response, "{\"temp\":%.1f,\"hum\":%d,\"flame\":%d,\"shock\":%d,\"state\":%d}",
						g_SensorData.PrecisionTemp, g_SensorData.Humidity, g_SensorData.FlameFlag, g_SensorData.ShockFlag, (int)g_DeviceStatus.State);
				CmdParser_PublishWithRetry(MQTT_TOPIC_RESP, Response);
			}
			break;

		case CMD_AI_ANALYZE:
			if(CurrentCmd.IsOneNetService == 1)
			{
				sprintf(Response, "{\"id\":\"%s\",\"code\":200,\"msg\":\"success\",\"data\":{}}", CurrentCmd.MsgId);
				CmdParser_PublishWithRetry(CurrentCmd.ReplyTopic, Response);
			}
			else
			{
				sprintf(Response, "{\"temp\":%.1f,\"hum\":%d,\"flame\":%d,\"shock\":%d}",
						g_SensorData.PrecisionTemp, g_SensorData.Humidity, g_SensorData.FlameFlag, g_SensorData.ShockFlag);
				CmdParser_PublishWithRetry(MQTT_TOPIC_DATA, Response);
			}
			break;

		case CMD_SET_THRESHOLD:
			if(CurrentCmd.IsOneNetService == 1)
			{
				sprintf(Response, "{\"id\":\"%s\",\"code\":200,\"msg\":\"success\",\"data\":{}}", CurrentCmd.MsgId);
				CmdParser_PublishWithRetry(CurrentCmd.ReplyTopic, Response);
			}
			else
			{
				sprintf(Response, "{\"result\":\"ok\",\"temp_threshold\":%d,\"hum_threshold\":%d}",
						(int)CurrentCmd.Param1, (int)CurrentCmd.Param2);
				CmdParser_PublishWithRetry(MQTT_TOPIC_RESP, Response);
			}
			break;

		case CMD_AI_RESULT:
			strncpy(g_DeviceStatus.AiResultText, CurrentCmd.Extra, 63);
			g_DeviceStatus.AiResultText[63] = '\0';
			g_DeviceStatus.AiResultNew = 1;
			sprintf(Response, "{\"result\":\"ok\",\"cmd\":\"ai_result_received\"}");
			CmdParser_PublishWithRetry(MQTT_TOPIC_RESP, Response);
			break;

		case CMD_ONENET_SET:
			CmdParser_ExecuteOneNetSet();
			break;

		default:
			CmdParser_PublishResult(Cmd, 400, "unknown_command");
			break;
	}
}

void CmdParser_SendResponse(char *Msg)
{
	char Response[128];
	sprintf(Response, "{\"result\":\"ok\",\"msg\":\"%s\"}", Msg);
	CmdParser_PublishWithRetry(MQTT_TOPIC_RESP, Response);
}
