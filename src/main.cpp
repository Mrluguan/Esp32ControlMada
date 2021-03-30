#include <Preferences.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cacert.h"
#include "staticconfig.h"
#include <ESPAsyncWebServer.h>
#include <EPD_display.h>
#include <DotsMqttMessage.h>
#include <CRC.h>
#include "esp_pm.h"
#include "Arduino.h"
#include <stdio.h>
#include <esp_wifi.h>
#include <Update.h>

char *CurrentPubTopic = new char[16 + 1 + 32 + 1];
char *CurrentSubTopic = new char[16 + 1 + 32 + 1];
char *CurrentBindingID = new char[32 + 1];
char *CurrentListenBindingIDPath = new char[10 + 8 + 1 + 1];
char *CurrentDeviceID = new char[10 + 8 + 1];

HTTPClient http;
AsyncWebServer httpserver(8080);

int KEY_FLAG = 0;
int LED_STATE = 0;
int LED_FLASH_FLAG = 0;

WiFiClientSecure espClient;
PubSubClient client(espClient);

bool keyDownWorking = false;
bool smartConfigWorking = false;
bool hasDeviceBinded = false;
bool needReconnectMqtt = true;
long LastReportTime = 0;

String PrefSSID;
String PrefPassword;
String BindingID;

//初始化GPIO
void initGPIO();
//OTA在线升级
void otaUpdate();
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
//进入配网模式
void smartConfigWIFI();
//等待客户端发送bindingID
void waitBindingID();
//初始化MQTT
void initMqtt();
//MQTT重连
void reconnectMqtt();
//发送回复消息
void SendRepeatMessage(unsigned char *RepeatID);
//获取当前剩余电量
const char *GetCurrentPowerLevel();
//报告当前设备状态
void ReportStatus();

void setup()
{
    delay(10);
    randomSeed(micros());
    LastReportTime = -1;

    Serial.begin(115200);

    setCpuFrequencyMhz(80);

    uint64_t chipid = ESP.getEfuseMac(); //The chip ID is essentially its MAC address(length: 6 bytes).
    unsigned char value[sizeof(chipid)];
    std::memcpy(value, &chipid, sizeof(chipid));
    uint crcRes = CRC32(value, sizeof(chipid));
    strcpy(CurrentDeviceID, CompanyNo);
    strcat(CurrentDeviceID, DeviceCategory);
    strcat(CurrentDeviceID, DeviceModelNo);
    sprintf(CurrentDeviceID + 10, "%-4X", crcRes);
    Serial.printf("CurrentDeviceID = %s\n", CurrentDeviceID);

    strcpy(CurrentListenBindingIDPath, "/");
    strcat(CurrentListenBindingIDPath, CurrentDeviceID);
    Serial.printf("CurrentListenBindingIDPath = %s\n", CurrentListenBindingIDPath);

    initGPIO();

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

        strcpy(CurrentPubTopic, mqtt_device_pub_topic_prefix);
        strcat(CurrentPubTopic, CurrentBindingID);
        Serial.printf("CurrentPubTopic = %s\n", CurrentPubTopic);

        strcpy(CurrentSubTopic, mqtt_device_sub_topic_prefix);
        strcat(CurrentSubTopic, CurrentBindingID);
        Serial.printf("CurrentSubTopic = %s\n", CurrentSubTopic);

        initWifi();
        if (PowerSaveMode)
        {

        }
        else
        {
            initMqtt();
        }
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
        delay(1000);
        initWifi();
    }
    if (hasDeviceBinded)
    {
        if (!client.connected())
        {
            LED_STATE = 0;
            if (needReconnectMqtt)
            {
                delay(1000);
                reconnectMqtt();
            }
        }
        else
        {
            LED_STATE = 1;
            long now = millis();
            if (LastReportTime < 0 || (now > LastReportTime && now - LastReportTime > status_report_interval) || (now < LastReportTime && LastReportTime - now > status_report_interval))
            {
                Serial.println(ESP.getFreeHeap());
                LastReportTime = now;
                ReportStatus();
            }
        }
        client.loop();
    }
    /*esp_sleep_enable_timer_wakeup(5000000);
    Serial.println("sleep");
    delay(500);
    esp_light_sleep_start();
    Serial.println("wakeup");*/
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
    esp_wifi_set_ps(WIFI_PS_MAX_MODEM);
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

void mqtt_callback(char *topic, byte *payload, unsigned int length)
{
    Serial.println("Received Mqtt Message");
    DotsMqttMessage WaitHandleCommand;
    WaitHandleCommand.FromPayload((unsigned char *)payload);
    Serial.println("Load Mqtt Message Success");
    std::string type = WaitHandleCommand.GetString("type");
    Serial.println(type.c_str());
    if (type == "refresh")
    {
        Serial.println("RefreshImage");
        Serial.println(WaitHandleCommand.GetValue("img").len);
        digitalWrite(16, LOW);
        EPD_init(); //EPD init
        PIC_display1(WaitHandleCommand.GetValue("img").item);
        EPD_refresh(); //EPD_refresh
        EPD_sleep();   //EPD_sleep,Sleep instruction is necessary, please do not delete!!!
        digitalWrite(16, HIGH);
        SendRepeatMessage(WaitHandleCommand.GetValue("repeat_id").item);
    }
}

void initMqtt()
{
    espClient.setCACert(mqtt_ca_cert);
    client.setBufferSize(65535);
    client.setServer(mqtt_server, mqtt_port);
    client.connect(CurrentBindingID, CurrentDeviceID, "");
    Serial.println("Mqtt Connected!");
    client.subscribe(CurrentSubTopic);
    client.setCallback(mqtt_callback);
}

void reconnectMqtt()
{
    Serial.println("reconnect MQTT...");
    if (client.connect(CurrentBindingID, CurrentDeviceID, ""))
    {
        client.subscribe(CurrentSubTopic);
        client.setCallback(mqtt_callback);
        Serial.println("connected");
    }
    else
    {
        Serial.println("failed");
        int state = client.state();
        Serial.println(state);
        if (state == 4)
        {
            Serial.println("Bad Mqtt Login Info.Not Reconnect");
            needReconnectMqtt = false;
            return;
        }
        Serial.println("try again in 5 sec");
        delay(5000);
    }
}

const char *GetCurrentPowerLevel()
{
    return "1";
}

void ReportStatus()
{
    if (client.connected())
    {
        Serial.println("Report Status");
        DotsMqttMessage msg;
        msg.SetString("type", "status");
        msg.SetString("status", "{\"power\":0}");
        OutputPayload output = msg.ToPayload();
        client.publish(CurrentPubTopic, output.data, output.length);
    }
}

void SendRepeatMessage(unsigned char *RepeatID)
{
    Serial.println("SendRepeatMessage");
    DotsMqttMessage msg;
    msg.SetString("type", "command_result");
    msg.SetByteArray("repeat_id", RepeatID, 16);
    msg.SetString("result", "success");
    OutputPayload output = msg.ToPayload();
    client.publish(CurrentPubTopic, output.data, output.length);
}
