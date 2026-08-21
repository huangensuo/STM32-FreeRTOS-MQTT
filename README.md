# STM32 智能环境监测与安全预警系统

这是一个基于 **STM32F103C8T6 + FreeRTOS + ESP8266 + OneNET + 微信小程序** 的智能环境监测与安全预警系统。系统采集温湿度、烟雾、火焰和人体红外数据，通过 MQTT 接入 OneNET 物联网平台，并支持微信小程序远程查看与控制。

本仓库定位为 **项目源码 + 简历展示文档**。源码保留主工程、微信小程序、云函数、可选服务器端和工具脚本；旧版交付文档、临时文件和历史归档已清理。

## 项目亮点

- 使用 FreeRTOS 拆分传感器采集、状态控制、OLED 显示、按键处理和 MQTT 通信任务。
- 支持 DHT11、MQ2、火焰传感器、HC-SR312 人体红外等多传感器数据采集。
- 设计正常、预警、危险三级安全状态机，实现本地声光报警和风扇控制。
- MQ2 烟雾检测支持上电预热、动态基准校准和去抖判断，降低误报警概率。
- ESP8266 通过 MQTT 接入 OneNET 物模型，实现属性上报和服务调用。
- 微信小程序通过云函数代理访问 OneNET，避免前端暴露 Access Key。
- 适合嵌入式、物联网、STM32、FreeRTOS 方向的简历项目展示。

## 功能说明

- 环境数据采集：温度、湿度、烟雾 ADC、火焰状态、人体检测状态。
- 本地安全预警：正常、预警、危险三级状态自动判断。
- 本地执行器控制：RGB、蜂鸣器、继电器风扇、照明 LED、OLED。
- OneNET 数据上报：通过 MQTT 上报设备属性。
- 远程控制：通过 OneNET 服务调用控制继电器、蜂鸣器、LED、RGB 和阈值。
- 微信小程序查看：实时查看设备数据和安全状态。

## 技术栈

| 层级 | 技术 |
| --- | --- |
| 主控 | STM32F103C8T6 |
| 固件 | C、STM32 标准外设库、FreeRTOS |
| 通信 | USART、ESP8266 AT 指令、MQTT |
| 云平台 | OneNET 物模型 |
| 应用端 | 微信小程序、微信云函数 |
| 可选后端 | Python、FastAPI、paho-mqtt |

## 目录结构

```text
Hardware/       STM32 外设驱动、传感器、执行器、MQTT 模块
User/           主函数、FreeRTOS 任务、系统状态和阈值配置
System/         延时等系统辅助模块
FreeRTOS/       FreeRTOS 内核源码
Library/        STM32F10x 标准外设库
Start/          启动文件和 CMSIS 文件
微信小程序/      小程序页面、工具函数和云函数源码
Server/         可选 Python 服务端源码
tools/          OneNET Token 生成等工具脚本
docs/           中文项目说明文档
Project.uvprojx Keil 主工程文件
```

## 快速开始

### 1. 配置固件联网参数

复制配置模板：

```text
Hardware/MQTT_Config_Template.h → Hardware/MQTT_Config.h
```

然后填写自己的：

- WiFi 名称
- WiFi 密码
- OneNET 产品 ID
- OneNET 设备名
- MQTT Broker
- MQTT Token
- MQTT Topic

`Hardware/MQTT_Config.h` 是本地私有配置文件，不应提交到 GitHub。

### 2. 编译烧录

使用 Keil MDK 打开：

```text
Project.uvprojx
```

编译后烧录到 STM32F103C8T6。

### 3. 配置 OneNET

参考：

```text
docs/03_部署与复现.md
docs/04_通信协议.md
```

创建产品、设备、属性和服务。

### 4. 部署微信小程序

使用微信开发者工具打开：

```text
微信小程序/
```

配置自己的云开发环境 ID，部署 `onenetProxy` 云函数，并设置 OneNET 环境变量。

## 文档说明

```text
docs/01_项目总览.md
docs/02_系统架构.md
docs/03_部署与复现.md
docs/04_通信协议.md
docs/05_测试报告.md
docs/06_安全说明.md
docs/07_简历描述.md
```

## 简历项目一句话

基于 STM32F103C8T6、FreeRTOS、ESP8266 和 OneNET 设计智能环境监测与安全预警系统，实现多传感器采集、本地声光报警、MQTT 物模型上报和微信小程序远程控制。

## 安全提醒

公开上传 GitHub 前，请确认仓库中不包含 WiFi 密码、OneNET Token、OneNET Access Key、设备密钥、云开发私有配置或 `.env` 文件。