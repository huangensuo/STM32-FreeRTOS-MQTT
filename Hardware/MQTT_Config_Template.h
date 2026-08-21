#ifndef __MQTT_CONFIG_TEMPLATE_H
#define __MQTT_CONFIG_TEMPLATE_H

/*
 * STM32 固件联网配置模板。
 *
 * 使用方法：
 * 1. 复制本文件为 Hardware/MQTT_Config.h。
 * 2. 将下面的占位值替换为自己的 WiFi 和 OneNET 参数。
 * 3. 不要把 Hardware/MQTT_Config.h 提交到公开仓库。
 */

/* WiFi 网络配置 */
#define WIFI_SSID      "your_wifi_ssid"
#define WIFI_PASSWORD  "your_wifi_password"
#define WIFI_PASS      WIFI_PASSWORD

/* OneNET 设备配置 */
#define ONENET_PRODUCT_ID  "your_product_id"
#define ONENET_DEVICE_NAME "stm32_env_monitor"
#define MQTT_BROKER        "your_product_id.mqtts.acc.cmcconenet.cn"
#define MQTT_PORT          1883
#define MQTT_CLIENT_ID     ONENET_DEVICE_NAME
#define MQTT_USERNAME      ONENET_PRODUCT_ID

/* MQTT_PASSWORD 是 OneNET MQTT 登录 Token，请使用 tools/generate-onenet-token.js 生成。 */
#define MQTT_PASSWORD      "your_onenet_mqtt_token"

/* MQTT 主题定义 */
#define MQTT_TOPIC_DATA        "$sys/your_product_id/stm32_env_monitor/thing/property/post"
#define MQTT_TOPIC_POST_REPLY  "$sys/your_product_id/stm32_env_monitor/thing/property/post/reply"
#define MQTT_TOPIC_SET         "$sys/your_product_id/stm32_env_monitor/thing/property/set"
#define MQTT_TOPIC_SET_REPLY   "$sys/your_product_id/stm32_env_monitor/thing/property/set_reply"
#define MQTT_TOPIC_SERVICE_PREFIX "$sys/your_product_id/stm32_env_monitor/thing/service/"
#define MQTT_TOPIC_RELAY_INVOKE   "$sys/your_product_id/stm32_env_monitor/thing/service/relay/invoke"
#define MQTT_TOPIC_BUZZER_INVOKE  "$sys/your_product_id/stm32_env_monitor/thing/service/buzzer/invoke"
#define MQTT_TOPIC_LED_INVOKE     "$sys/your_product_id/stm32_env_monitor/thing/service/led/invoke"
#define MQTT_TOPIC_RGB_INVOKE     "$sys/your_product_id/stm32_env_monitor/thing/service/rgb/invoke"
#define MQTT_TOPIC_STATUS_INVOKE  "$sys/your_product_id/stm32_env_monitor/thing/service/get_status/invoke"
#define MQTT_TOPIC_AI_INVOKE      "$sys/your_product_id/stm32_env_monitor/thing/service/ai_analyze/invoke"
#define MQTT_TOPIC_TEMP_INVOKE    "$sys/your_product_id/stm32_env_monitor/thing/service/temp_threshold/invoke"
#define MQTT_TOPIC_HUM_INVOKE     "$sys/your_product_id/stm32_env_monitor/thing/service/hum_threshold/invoke"
#define MQTT_TOPIC_HISTORY_INVOKE "$sys/your_product_id/stm32_env_monitor/thing/service/upload_history/invoke"

#endif /* __MQTT_CONFIG_TEMPLATE_H */