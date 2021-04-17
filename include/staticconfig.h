#ifndef _STATIC_CONFIG
#define _STATIC_CONFIG

const int TimeZone = 8;
//主要服务器地址
const char *mqtt_server = "iot.dotsourceit.com";
//公司代号
const char *CompanyNo = "DY";
//产品类型
const char *DeviceCategory = "EID";
//产品型号
const char *DeviceModelNo = "MP072";

//PING间隔秒数
#define PING_INTERVAL_SEC 6
//ADC电压测量
#define ADC_PIN 35
//重置按钮
#define RESETKEY_PIN 4
//红色LED
#define LED_RED_PIN 18
//绿色LED
#define LED_GREEN_PIN 17
//屏幕电源
#define EPD_POWER 16
#endif