#ifndef __LLM_H
#define __LLM_H

#include "stm32f10x.h"

#define LLM_API_KEY      "your_api_key_here"
#define LLM_MODEL        "gpt-3.5-turbo"
#define LLM_HOST         "api.openai.com"
#define LLM_PORT         80
#define LLM_TIMEOUT      15000

#define LLM_MAX_PROMPT   512
#define LLM_MAX_RESPONSE 1024

typedef enum {
    LLM_STATUS_IDLE = 0,
    LLM_STATUS_CONNECTING,
    LLM_STATUS_SENDING,
    LLM_STATUS_RECEIVING,
    LLM_STATUS_COMPLETED,
    LLM_STATUS_ERROR
} LLM_StatusTypeDef;

extern LLM_StatusTypeDef LLM_Status;
extern char LLM_Response[LLM_MAX_RESPONSE];

uint8_t LLM_Init(void);
uint8_t LLM_SendPrompt(char *Prompt, char *Response);
uint8_t LLM_AnalyzeEnvironment(float Temp, float Hum, uint16_t FlameValue, uint8_t ShockFlag, char *Analysis);

#endif
