/*
 * DHT11温湿度传感器驱动（单总线协议）
 * 引脚：PB15（单总线）
 * 功能：采集空气温湿度，分辨率为1°C和1%RH
 * 数据格式：40位数据，包含湿度整数、湿度小数（通常为0）、温度整数、温度小数（通常为0）、校验和
 * 注意：DHT11小数位通常为0，实际有效数据为整数部分
 */
#ifndef __DHT11_H_
#define __DHT11_H_

#define DHT11_PIN    GPIO_Pin_15  // DHT11单总线引脚
#define DHT11_GPIO   GPIOB        // DHT11端口

void DHT11_OUT_Mode(void);                 // 设置引脚为输出模式
void DHT11_IN_Mode(void);                  // 设置引脚为输入模式
uint8_t DHT11_ReadByte(void);              // 读取一个字节
uint8_t DHT11_ReadData(uint8_t *hum, uint8_t *temp);  // 读取温湿度数据

#endif
