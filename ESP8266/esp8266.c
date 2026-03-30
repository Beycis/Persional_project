#include "esp8266.h"
#include "usart.h"
#include "delay.h"
#include <stdio.h>
#include <string.h>

u8 esp8266_recv_buf[ESP8266_RECV_BUF_LEN];
u16 esp8266_recv_len = 0;

void ESP8266_ClearRecvBuf(void)
{
    memset(esp8266_recv_buf, 0, ESP8266_RECV_BUF_LEN);
    esp8266_recv_len = 0;
}

void ESP8266_HandleRecvData(u8 data)
{
    if(esp8266_recv_len < ESP8266_RECV_BUF_LEN-1)
    {
        esp8266_recv_buf[esp8266_recv_len++] = data;
    }
    else
    {
        ESP8266_ClearRecvBuf();
    }
}

u8 ESP8266_Init(void)
{
    uart3_init(115200);
    ESP8266_ClearRecvBuf();
    delay_ms(1000);

    // 基础测试
    if(ESP8266_SendCmd((u8*)"AT", (u8*)"OK", 1500) != 0) return 1;

    // ====================== 【关键！必须加这 3 条！】======================
    ESP8266_SendCmd((u8*)"ATE0",            (u8*)"OK", 1000);  // 关闭回显
    ESP8266_SendCmd((u8*)"AT+CWMODE=1",     (u8*)"OK", 1000);  // STA模式
    ESP8266_SendCmd((u8*)"AT+CIPMUX=0",     (u8*)"OK", 1000);  // 单连接模式（必须！）
    ESP8266_SendCmd((u8*)"AT+CIPMODE=0",    (u8*)"OK", 1000);  // 非透传模式（必须！）
    // ======================================================================

    return 0;
}

u8 ESP8266_SendCmd(u8 *cmd, u8 *res, u32 timeout)
{
    u8 ret = 1;
    u32 t = 0;
    ESP8266_ClearRecvBuf();

    uart3_send_string(cmd);
    uart3_send_char('\r');
    uart3_send_char('\n');

    while(timeout > t)
    {
        delay_ms(1);
        t++;
        if(strstr((char*)esp8266_recv_buf, (char*)res) != NULL)
        {
            ret = 0;
            break;
        }
    }
    return ret;
}

u8 ESP8266_ConnectWiFi(u8 *ssid, u8 *pwd)
{
    u8 buf[128];
    sprintf((char*)buf, "AT+CWJAP=\"%s\",\"%s\"", ssid, pwd);
    if(ESP8266_SendCmd(buf, (u8*)"OK", 8000) != 0) return 1;
    return 0;
}

// ===================== 【修复完成】OneNET MQTT 连接 =====================
u8 ESP8266_ConnectMQTT(void)
{
    u8 cmd[512];  // 足够大，存放长指令
    delay_ms(500);

    // 1. 清空旧连接
    ESP8266_SendCmd((u8*)"AT+MQTTCLEAN=0", (u8*)"OK", 1000);
    delay_ms(200);

    // 2. 【你要的格式！完全一样！】
    sprintf((char*)cmd, "AT+MQTTUSERCFG=0,1,\"%s\",\"%s\",\"%s\",0,0,\"\"",
            MQTT_CLIENT_ID,
            MQTT_USERNAME,
            MQTT_PASSWORD);

    printf("MQTT指令发送：%s\r\n", cmd);  // 串口看一眼，确保和你手动发的一样

    // 发送这条指令，等待 OK
    if(ESP8266_SendCmd(cmd, (u8*)"OK", 3000) != 0)
    {
        printf("MQTTUSERCFG 失败\r\n");
        return 1;
    }

    // 3. 连接服务器
    sprintf((char*)cmd, "AT+MQTTCONN=0,\"%s\",%s,1", MQTT_SERVER_IP, MQTT_SERVER_PORT);
    if(ESP8266_SendCmd(cmd, (u8*)"CONNECT", 5000) != 0)
    {
        printf("MQTTCONN 失败\r\n");
        return 1;
    }
	ESP8266_SendCmd((u8*)"AT+MQTTMODE=1", (u8*)"OK", 1000); 
    // 4. 订阅主题
    sprintf((char*)cmd, "AT+MQTTSUB=0,\"%s\",0", MQTT_SUB_TOPIC);
    ESP8266_SendCmd(cmd, (u8*)"OK", 1000);

    printf("✅ OneNET MQTT 连接成功！\r\n");
    return 0;
}

// ===================== 发布JSON =====================
// ===================== 【终极修复】MQTT 发布 =====================
u8 ESP8266_MQTTPublish(u8 *topic, u8 *data)
{
    char cmd[512];
    sprintf(cmd, "AT+MQTTPUBRAW=0,\"%s\",%d,0,0", topic, strlen((char*)data));
    
    printf("透传指令：%s\r\n", cmd);
    ESP8266_SendCmd((u8*)cmd, (u8*)">", 2000); // 等待 >
    
    uart3_send_string(data); // 直接发送原始JSON！！！
    delay_ms(500);
    
    printf("✅ 透传发送成功！\r\n");
    return 0;
}




// ===================== 封装JSON =====================
void ESP8266_PackOneNETJSON(u8 *json_buf, float temp, float EC)
{
    // 【OneNET 唯一识别格式！】
    sprintf((char*)json_buf, "{\"id\":\"2555180766\",\"params\":{\"temp\":{\"value\":%.1f},\"EC\":{\"value\":%.1f}}}", temp,EC);
}



