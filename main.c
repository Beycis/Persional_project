#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "led.h"
#include "lcd.h"
#include "ds18b20.h"
#include "usart3.h"
#include "esp8266.h"
#include "TDS.h"

int main(void)
{
    u8 t = 0;
    short temperature;
    u8 json_buf[512];
    float temp;
    float EC;  // 电导率值

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    delay_init(168);
    uart_init(115200);
    uart3_init(115200);

    LED_Init();
    LCD_Init();
    ADC1_Init();  // TDS初始化

    POINT_COLOR = RED;
    LCD_ShowString(30,50,200,16,16,"Explorer STM32F4");
    LCD_ShowString(30,70,200,16,16,"DS18B20 TEST");
    LCD_ShowString(30,90,200,16,16,"ESP8266 + MQTT");

    // DS18B20
    while(DS18B20_Init())
    {
        LCD_ShowString(30,130,200,16,16,"DS18B20 Error");
        delay_ms(200);
    }
    LCD_ShowString(30,130,200,16,16,"DS18B20 OK");

    // ESP8266
    while(ESP8266_Init()==1)
    {
        LCD_ShowString(30,150,200,16,16,"ESP8266 Error");
        delay_ms(200);
    }
    LCD_ShowString(30,150,200,16,16,"ESP8266 OK");

    // WIFI
    u8 wifi_ssid[] = "Explain";
    u8 wifi_pwd[] = "15886598338";
    LCD_ShowString(30,170,200,16,16,"Temp:   . C");
		LCD_ShowString(30, 190, 200, 16, 16, "TDS:    . ppm");
    if(ESP8266_ConnectWiFi(wifi_ssid, wifi_pwd) == 0)
    {
        LCD_ShowString(30,210,200,16,16,"WiFi OK");
        if(ESP8266_ConnectMQTT() == 0)
        {
            LCD_ShowString(30,230,200,16,16,"MQTT OK");
        }
        else
        {
            LCD_ShowString(30,230,200,16,16,"MQTT ERR");
        }
    }

    POINT_COLOR = BLUE;

    while(1)
    {
        // --------------------- 100ms 刷新 ---------------------
        if(t%10 == 0)
        {
            // 温度
            temperature = DS18B20_Get_Temp();
            temp = (float)temperature / 10.0f;

            if(temperature < 0)
            {
                LCD_ShowChar(30+40,170,'-',16,0);
                temperature = -temperature;
            }
            else
            {
                LCD_ShowChar(30+40,170,' ',16,0);
            }
            LCD_ShowNum(30+48,170,temperature/10,2,16);
            LCD_ShowNum(30+72,170,temperature%10,1,16);

            // ====================== ✅ 读取 TDS ======================
            EC = Get_TDS_Value(temp);  // 每次都读最新值
						printf("当前TDS：%.1f\r\n", EC);
					
        }

        // ====================== ✅ 实时显示 TDS（不卡！） ======================
        u16 tds_int = (u16)EC;
        u16 tds_dec = (u16)(EC * 10) % 10;
        LCD_ShowNum(30+38, 190, tds_int,  3, 16);   // ✅ 修复：不再除以10！
        LCD_ShowNum(30+72, 190, tds_dec%10,  1, 16);

        // ====================== ✅ 2秒发送（用最新值！） ======================
        if(t % 200 == 0)
        {
            EC = Get_TDS_Value(temp);  // ✅ 发送前强制重新读取！
            printf("当前温度：%.1f ℃ | TDS：%.1f\r\n", temp, EC);
            ESP8266_PackOneNETJSON(json_buf, temp, EC);
            printf("发送JSON：%s\r\n", json_buf);
            ESP8266_MQTTPublish((u8*)MQTT_PUB_TOPIC, json_buf);
        }

        delay_ms(10);
        t++;
        if(t >= 1000) t = 0;

        if(t%20 == 0) LED0 = !LED0;
    }
}