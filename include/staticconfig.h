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
const int ping_req_interval_sec = 6;

/*const int POWER_EN = 15;
const int LED_RED_PIN = 5;
const int LED_GREEN_PIN = 17;
const int LED_BLUE_PIN = 16;*/

const int POWER_EN = 17;
const int LED_RED_PIN = 17;
const int LED_GREEN_PIN = 17;
const int LED_BLUE_PIN = 17;

const int KEY = 4;

#endif