#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "staticconfig.h"
#include <ESPAsyncWebServer.h>
#include <EPD_display.h>
#include <CRC.h>
#include "Arduino.h"
#include <stdio.h>
#include <esp_wifi.h>
#include "OTA.h"
#include "time.h"

char *CurrentBindingID = new char[32 + 1];
char *CurrentListenBindingIDPath = new char[10 + 8 + 1 + 1];
char *CurrentDeviceID = new char[10 + 8 + 1];

HTTPClient httpClient;
AsyncWebServer httpserver(8080);

int KEY_FLAG = 0;
int LED_STATE = 0;
int LED_FLASH_FLAG = 0;

bool keyDownWorking = false;
bool smartConfigWorking = false;
bool hasDeviceBinded = false;
long LastReportTime = 0;
long LastHttpPingTime = 0;

String PrefSSID;
String PrefPassword;
String BindingID;

//初始化全局参数
void initParameters();
//初始化GPIO
void initGPIO();
//刷新LED
void refreshLED(void *Parameter);
//监控按键
void keyMonitor(void *Parameter);
//按键事件
void keyUp();
//重置设备
void resetAll();
//初始化wifi
void initWifi();
//时间同步
void sntpAysnc();
//进入配网模式
void smartConfigWIFI();
//等待客户端发送bindingID
void waitBindingID();
//获取当前剩余电量
const char *GetCurrentPowerLevel();

void setup()
{
    delay(10);
    randomSeed(micros());
    Serial.begin(115200);
    setCpuFrequencyMhz(80);

    initGPIO();
    initParameters();

    //LED刷新任务
    xTaskCreatePinnedToCore((TaskFunction_t)refreshLED, "refreshLED", 1024, (void *)NULL, (UBaseType_t)2, (TaskHandle_t *)NULL, (BaseType_t)tskNO_AFFINITY);
    //按钮监控任务
    xTaskCreatePinnedToCore((TaskFunction_t)keyMonitor, "keyMonitor", 4096, (void *)NULL, (UBaseType_t)2, (TaskHandle_t *)NULL, (BaseType_t)tskNO_AFFINITY);

    //read wifi in NVS
    Preferences prefs;
    prefs.begin("options", false);
    PrefSSID = prefs.getString("ssid", "none");
    PrefPassword = prefs.getString("password", "none");
    BindingID = prefs.getString("BindingID", "none");
    prefs.end();

    if (BindingID == "none")
    {
        Serial.println("BindingID Not Found");
        return;
    }
    else
    {
        memcpy(CurrentBindingID, BindingID.c_str(), 33);
        Serial.printf("CurrentBindingID = %s\n", CurrentBindingID);
        hasDeviceBinded = true;
        initWifi();
    }
}

void loop()
{
    //return;
    if (smartConfigWorking)
        LED_STATE = 2;
    if (!smartConfigWorking && BindingID != "none" && WiFi.status() != WL_CONNECTED)
    {
        LED_STATE = 0;
        delay(100);
        initWifi();
    }
    if (hasDeviceBinded)
    {
        LED_STATE = 1;
        long now = millis();
        if (LastReportTime < 0 || (now > LastReportTime && now - LastReportTime > status_report_interval) || (now < LastReportTime && LastReportTime - now > status_report_interval))
        {
            Serial.println(ESP.getFreeHeap());
            LastReportTime = now;
            
        }
    }
}

void initParameters()
{
    LastReportTime = -1;
    LastHttpPingTime = -1;
    uint64_t chipid = ESP.getEfuseMac(); //The chip ID is essentially its MAC address(length: 6 bytes).
    unsigned char value[sizeof(chipid)];
    memcpy(value, &chipid, sizeof(chipid));
    uint crcRes = CRC32(value, sizeof(chipid));
    strcpy(CurrentDeviceID, CompanyNo);
    strcat(CurrentDeviceID, DeviceCategory);
    strcat(CurrentDeviceID, DeviceModelNo);
    sprintf(CurrentDeviceID + 10, "%-4X", crcRes);
    Serial.printf("CurrentDeviceID = %s\n", CurrentDeviceID);

    strcpy(CurrentListenBindingIDPath, "/");
    strcat(CurrentListenBindingIDPath, CurrentDeviceID);
    Serial.printf("CurrentListenBindingIDPath = %s\n", CurrentListenBindingIDPath);
}

void initGPIO()
{
    pinMode(16, OUTPUT);
    digitalWrite(16, 1);

    pinMode(BUSY_Pin, INPUT);
    pinMode(RES_Pin, OUTPUT);
    pinMode(DC_Pin, OUTPUT);
    pinMode(CS_Pin, OUTPUT);
    pinMode(SCK_Pin, OUTPUT);
    pinMode(SDI_Pin, OUTPUT);

    pinMode(LED_RED_PIN, OUTPUT);
    pinMode(LED_GREEN_PIN, OUTPUT);
    pinMode(LED_BLUE_PIN, OUTPUT);

    digitalWrite(LED_RED_PIN, HIGH);

    pinMode(KEY, INPUT | PULLUP);
    attachInterrupt(KEY, keyUp, RISING);
}

void refreshLED(void *Parameter)
{
    while (1)
    {
        //0=red,1=blue,2=red flash
        if (LED_STATE == 0)
        {
            digitalWrite(LED_RED_PIN, HIGH);
            digitalWrite(LED_BLUE_PIN, LOW);
            digitalWrite(LED_GREEN_PIN, LOW);
        }
        else if (LED_STATE == 1)
        {
            digitalWrite(LED_BLUE_PIN, HIGH);
            digitalWrite(LED_RED_PIN, LOW);
            digitalWrite(LED_GREEN_PIN, LOW);
        }
        else if (LED_STATE == 2)
        {
            if (LED_FLASH_FLAG == 0)
            {
                digitalWrite(LED_RED_PIN, LOW);
                digitalWrite(LED_BLUE_PIN, LOW);
                digitalWrite(LED_GREEN_PIN, LOW);
                LED_FLASH_FLAG = 1;
            }
            else if (LED_FLASH_FLAG == 1)
            {
                digitalWrite(LED_RED_PIN, HIGH);
                digitalWrite(LED_BLUE_PIN, LOW);
                digitalWrite(LED_GREEN_PIN, LOW);
                LED_FLASH_FLAG = 0;
            }
        }
        vTaskDelay(500);
    }
}

void keyMonitor(void *Parameter)
{
    while (1)
    {
        if (!smartConfigWorking && KEY_FLAG == 1)
        {
            LED_STATE = 2;
            resetAll();
            smartConfigWIFI();
            waitBindingID();
            KEY_FLAG = 0;
        }
        vTaskDelay(150);
        delay(50);
    }
}

void keyUp()
{
    if (KEY_FLAG == 1)
        return;
    KEY_FLAG = 1;
}

void resetAll()
{
    Serial.println("重置设备");
    Preferences prefs;
    prefs.begin("options", false);
    prefs.clear();
    prefs.end();
    hasDeviceBinded = false;
    if (WiFi.status() == WL_CONNECTED)
    {
        WiFi.disconnect();
    }
}

void initWifi()
{
    Serial.println();
    Serial.print("Try to connection original wifi...");

    WiFi.mode(WIFI_STA);
    WiFi.begin(PrefSSID.c_str(), PrefPassword.c_str());

    //Try to connection 10 seconds
    int count = 0;
    while (WiFi.status() != WL_CONNECTED)
    {
        if (count == 20)
        {
            Serial.println("Connect wifi failed!");
        }
        delay(500);
        count++;
        Serial.print(".");
    }
    Serial.print("Connected! IP address: ");
    Serial.println(WiFi.localIP());
    sntpAysnc();
}

void sntpAysnc()
{
    configTime(TimeZone * 3600, 0, "ntp.aliyun.com");
    Serial.print("SntpAysnc Finish.Current datetime:");
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        Serial.println("Failed to obtain time");
        return;
    }
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

void smartConfigWIFI()
{
    Serial.println("进入配网模式");
    smartConfigWorking = true;

    WiFi.mode(WIFI_AP_STA);

    WiFi.beginSmartConfig();

    //Wait for SmartConfig packet from mobile
    Serial.println("Waiting for SmartConfig.");
    while (!WiFi.smartConfigDone())
    {
        delay(1000);
    }

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(1000);
    }

    Preferences prefs;
    PrefSSID = WiFi.SSID();
    PrefPassword = WiFi.psk();

    Serial.println("SSID=" + PrefSSID);
    Serial.println("Password=" + PrefPassword);

    prefs.begin("options", false);
    prefs.putString("ssid", WiFi.SSID());
    prefs.putString("password", WiFi.psk());
    prefs.end();

    Serial.println("SmartConfig done");
    delay(1000);
    smartConfigWorking = false;
}

void waitBindingID()
{
    Serial.println("等待bindingID");
    httpserver.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("http req");
        request->send(200, "application/json", "{\"code\":0}");
    });
    httpserver.on(CurrentListenBindingIDPath, HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("http req bind");
        String message;
        if (request->hasParam("msg"))
        {
            message = request->getParam("msg")->value();
            Serial.println("Received BindID");
            Preferences prefs;
            prefs.begin("options", false);
            prefs.putString("BindingID", message);
            prefs.end();
            request->send(200, "application/json", "{\"code\":0}");
        }
        else
        {
            request->send(200, "application/json", "{\"code\":25,\"error\":\"提交绑定参数错误\"}");
        }
        delay(500);
        httpserver.end();
        ESP.restart();
    });
    httpserver.begin();
}

const char *GetCurrentPowerLevel()
{
    return "1";
}
