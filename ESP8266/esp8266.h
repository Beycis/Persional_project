#ifndef __ESP8266_H
#define __ESP8266_H

#include "sys.h"
#include "usart3.h"
#include <string.h>

/************************* OneNET MQTT 配置 *************************/
#define MQTT_SERVER_IP        "mqtts.heclouds.com"
#define MQTT_SERVER_PORT      "1883"
#define MQTT_PRODUCT_ID       "7yQ81AC5c3"          // 你的产品ID
#define MQTT_DEVICE_NAME      "ESP8266"             // 设备名
#define MQTT_CLIENT_ID        "ESP8266"   // 产品ID+设备名
#define MQTT_USERNAME         "7yQ81AC5c3"
#define MQTT_PASSWORD         "version=2018-10-31&res=products%2F7yQ81AC5c3%2Fdevices%2FESP8266&et=1910271117&method=md5&sign=ntX6MD9GFRiybL9Ucl22Fg%3D%3D"

// OneNET 必用的上报主题
#define MQTT_PUB_TOPIC        "$sys/7yQ81AC5c3/ESP8266/thing/property/post"
#define MQTT_SUB_TOPIC        "$sys/7yQ81AC5c3/ESP8266/thing/property/post/reply"

/************************* 配置参数 *************************/
#define ESP8266_CMD_TIMEOUT    5000
#define ESP8266_RECV_BUF_LEN   1024
#define JSON_BUF_LEN           512

/************************* 函数声明 *************************/
u8 ESP8266_Init(void);
u8 ESP8266_SendCmd(u8 *cmd, u8 *res, u32 timeout);
u8 ESP8266_ConnectWiFi(u8 *ssid, u8 *pwd);
u8 ESP8266_ConnectMQTT(void);
u8 ESP8266_MQTTPublish(u8 *topic, u8 *data);
u8 ESP8266_MQTTSubscribe(u8 *topic);
void ESP8266_ClearRecvBuf(void);
void ESP8266_HandleRecvData(u8 data);

// 封装 OneNET 标准 JSON
void ESP8266_PackOneNETJSON(u8 *json_buf, float temp, float EC);

#endif
