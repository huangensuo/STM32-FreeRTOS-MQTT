/**
  ******************************************************************************
  * @file    Project/STM32F10x_StdPeriph_Template/stm32f10x_it.c 
  * @author  MCD Application Team
  * @version V3.5.0
  * @date    08-April-2011
  * @brief   Main Interrupt Service Routines.
  *          This file provides template for all exceptions handler and 
  *          peripherals interrupt service routine.
  ******************************************************************************
  * @attention
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * <h2><center>&copy; COPYRIGHT 2011 STMicroelectronics</center></h2>
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x_it.h"
#include "../Hardware/PIR.h"
#include "../Hardware/USART.h"

/* ── +IPD capture state machine ────────────────────── */
static uint8_t  ipd_state = 0;
static uint16_t ipd_idx   = 0;
static uint16_t ipd_total = 0;
static uint8_t  ipd_detect[5];
static uint8_t  ipd_detect_idx = 0;

/** @addtogroup STM32F10x_StdPeriph_Template
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/******************************************************************************/
/*            Cortex-M3 Processor Exceptions Handlers                         */
/******************************************************************************/

/**
  * @brief  This function handles NMI exception.
  * @param  None
  * @retval None
  */
void NMI_Handler(void)
{
}

/**
  * @brief  This function handles Hard Fault exception.
  * @param  None
  * @retval None
  */
void HardFault_Handler(void)
{
  /* Go to infinite loop when Hard Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Memory Manage exception.
  * @param  None
  * @retval None
  */
void MemManage_Handler(void)
{
  /* Go to infinite loop when Memory Manage exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Bus Fault exception.
  * @param  None
  * @retval None
  */
void BusFault_Handler(void)
{
  /* Go to infinite loop when Bus Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Usage Fault exception.
  * @param  None
  * @retval None
  */
void UsageFault_Handler(void)
{
  /* Go to infinite loop when Usage Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles SVCall exception.
  * @param  None
  * @retval None
  */
//void SVC_Handler(void)
//{
//}

/**
  * @brief  This function handles Debug Monitor exception.
  * @param  None
  * @retval None
  */
//void DebugMon_Handler(void)
//{
//}

/**
  * @brief  This function handles PendSVC exception.
  * @param  None
  * @retval None
  */
//void PendSV_Handler(void)
//{
//}

/**
  * @brief  This function handles SysTick Handler.
  * @param  None
  * @retval None
  * @note   SysTick_Handler is defined by FreeRTOS port.c via macro
  *         #define xPortSysTickHandler SysTick_Handler in FreeRTOSConfig.h
  */
/******************************************************************************/
/*                 STM32F10x Peripherals Interrupt Handlers                   */
/*  Add here the Interrupt Handler for the used peripheral(s) (PPP), for the  */
/*  available peripheral interrupt handler's name please refer to the startup */
/*  file (startup_stm32f10x_xx.s).                                            */
/******************************************************************************/

void EXTI1_IRQHandler(void)
{
    if(EXTI_GetITStatus(EXTI_Line1) != RESET)
    {
        for(volatile uint32_t i = 0; i < 10000; i++);
        if(EXTI_GetITStatus(EXTI_Line1) != RESET)
        {
            PIR_UpdateFlag();
        }
        EXTI_ClearITPendingBit(EXTI_Line1);
    }
}

/**
  * @brief  USART1 RX interrupt — receives data from ESP8266
  *
  * Two modes:
  *   1) Regular AT response → fills USART1_RxBuffer, sets USART1_RxFlag
 *   2) +IPD data capture  → fills IPD_Buffer, sets IPD_Ready
 */
void USART1_IRQHandler(void)
{
    uint8_t ch;
    uint8_t i;

    if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
    {
        ch = (uint8_t)USART_ReceiveData(USART1);

        /* ── Regular UART receive (AT command responses) ── */
        if(USART1_RxLen < USART1_RX_BUF_SIZE - 1)
        {
            USART1_RxBuffer[USART1_RxLen++] = ch;
            USART1_RxBuffer[USART1_RxLen] = '\0';
            USART1_RxFlag = 1;
        }

        /* ── +IPD detection & capture ──────────────────── */
        switch(ipd_state)
        {
            case 0:  /* Detect "+IPD," sequence (5-byte sliding shift register) */
                /* 移位寄存器：旧字节左移，新字节放入末尾 */
                for(i = 0; i < 4; i++)
                {
                    ipd_detect[i] = ipd_detect[i + 1];
                }
                ipd_detect[4] = ch;
                if(ipd_detect[0] == '+' && ipd_detect[1] == 'I' &&
                   ipd_detect[2] == 'P' && ipd_detect[3] == 'D' &&
                   ipd_detect[4] == ',')
                {
                    ipd_state = 1;
                    ipd_total = 0;
                    ipd_idx = 0;
                }
                /* 清除环状缓冲区的旧索引变量 */
                ipd_detect_idx = 0;
                break;

            case 1:  /* Parse data length until ':' */
                if(ch >= '0' && ch <= '9')
                {
                    ipd_total = ipd_total * 10 + (uint16_t)(ch - '0');
                }
                else if(ch == ':')
                {
                    ipd_idx = 0;
                    if(ipd_total <= IPD_BUF_SIZE)
                    {
                        ipd_state = 2;
                    }
                    else
                    {
                        ipd_state = 3;  /* Payload too large for IPD_Buffer, discard it safely */
                    }
                }
                else if(ch == ',')
                {
                    /* Second length field? Reset and re-read */
                    ipd_total = 0;
                }
                else
                {
                    /* Unexpected char, reset */
                    ipd_state = 0;
                }
                break;

            case 2:  /* Capture IPD data bytes */
                if(ipd_idx < IPD_BUF_SIZE)
                {
                    IPD_Buffer[ipd_idx++] = ch;
                }
                if(ipd_idx >= ipd_total)
                {
                    IPD_Len    = ipd_idx;
                    IPD_Ready  = 1;
                    USART1_SaveIPDCount++;
                    ipd_state  = 0;
                }
                break;

            case 3:  /* Discard oversized IPD payload */
                ipd_idx++;
                if(ipd_idx >= ipd_total)
                {
                    ipd_state = 0;
                    ipd_idx = 0;
                    ipd_total = 0;
                }
                break;

        }
    }
}

/**
  * @brief  This function handles PPP interrupt request.
  * @param  None
  * @retval None
  */
/*void PPP_IRQHandler(void)
{
}*/

/**
  * @}
  */ 


/******************* (C) COPYRIGHT 2011 STMicroelectronics *****END OF FILE****/
