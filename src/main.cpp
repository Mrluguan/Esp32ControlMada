#include <Preferences.h>
#include <WiFi.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "staticconfig.h"
#include <ESPAsyncWebServer.h>
#include <EPD_display.h>
#include <CRC.h>
#include "Arduino.h"
#include "OTA.h"
#include "time.h"
#include <ArduinoJson.h>
#include <esp_pm.h>
#include <driver/ledc.h>

char *CurrentBindingID = new char[32 + 1];
char *CurrentListenBindingIDPath = new char[10 + 8 + 1 + 1];
char *CurrentDeviceID = new char[10 + 8 + 1];

AsyncWebServer httpserver(8080);
WebAPI webApi;

int KEY_FLAG = 0;
int LED_STATE = 4; //0=红灯，1=绿灯，2=红灯闪烁，3=绿灯闪烁，4=红绿全亮，5=红绿全熄，6=红绿交替闪烁
int LED_FLASH_FLAG = 0;

int currentState = 0; //0=刚开机，1=无法连接，2=正在配网，3=正常工作，4=繁忙

bool hasDeviceBinded = false;
long LastHttpPingTime = 0;
long setupStartTime = 0;

int pingErrorCount = 0;

long sleepTime = 10000;
long LastSleepTime = 0;

long LastSntpSync = 0;

String PrefSSID = "azkiki";
String PrefPassword = "azkiki123.";
String BindingID = "test";

//初始化全局参数
void initParameters();
//初始化GPIO
void initGPIO();
//进入重置模式
void deviceSetup();
//刷新LED
void refreshLED(void *Parameter);
//监控按键
void keyMonitor(void *Parameter);
//按键事件
void keyUp();
//初始化wifi
void connectWifi();
//时间同步
void sntpAysnc();
//ping
void ping();
//获取当前设备状态字符串
String GetCurrentDeviceStatus();
//设备睡眠
void deviceSleep();
//设置当前状态0=刚开机，1=无法连接，2=正在配网，3=正常工作，4=繁忙
void setCurrentState(int state);
//处理命令
void handleCommand(String command);
//向网关标记繁忙状态
void setBusy(bool busy);

void setup()
{
    //开机
    delay(10);
    esp_pm_config_esp32_t pmset;
    pmset.light_sleep_enable = true;
    esp_err_t err = esp_pm_configure(&pmset);
    Serial.printf("esp_pm_configure failed: %s", esp_err_to_name(err));
    randomSeed(micros());
    Serial.begin(115200);
    setCpuFrequencyMhz(240);
    setCurrentState(0);

    uint32_t flash_size = ESP.getFlashChipSize();
    Serial.printf("flash size = %dMB\n", flash_size / 1024 / 1024);
    //初始化GPIO
    initGPIO();
    //初始化系统基础配置
    initParameters();
    //LED刷新任务
    xTaskCreatePinnedToCore((TaskFunction_t)refreshLED, "refreshLED", 1024, (void *)NULL, (UBaseType_t)2, (TaskHandle_t *)NULL, (BaseType_t)tskNO_AFFINITY);
    //按钮监控任务
    xTaskCreatePinnedToCore((TaskFunction_t)keyMonitor, "keyMonitor", 4096, (void *)NULL, (UBaseType_t)2, (TaskHandle_t *)NULL, (BaseType_t)tskNO_AFFINITY);

    if (BindingID == "")
    {
        hasDeviceBinded = false;
        Serial.println("device not setup");
        deviceSetup();
    }
    else
    {
        memcpy(CurrentBindingID, BindingID.c_str(), 33);
        Serial.printf("CurrentBindingID = %s\n", CurrentBindingID);
        hasDeviceBinded = true;
    }
}

void loop()
{
    if (millis() > 40ul * 24ul * 60ul * 60ul * 1000ul)
    {
        ESP.restart();
        return;
    }
    if (currentState == 2)
        return;
    if (hasDeviceBinded)
    {
        connectWifi();
        if (WiFi.isConnected())
        {
            if (millis() - LastSntpSync > 6 * 60 * 60 * 1000)
            {
                sntpAysnc();
                LastSntpSync = millis();
            }
            ping();
            if (pingErrorCount >= 3)
            {
                setCurrentState(1);
            }
            else
            {
                setCurrentState(3);
            }
        }
    }
    deviceSleep();
}

void initParameters()
{
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

    //read wifi in NVS
    Preferences prefs;
    prefs.begin("options", false);
    PrefSSID = prefs.getString("ssid", "");
    PrefPassword = prefs.getString("password", "");
    BindingID = prefs.getString("BindingID", "");
    prefs.end();
}

/*#define RED_LED_OPEN ledcWrite(0, 200)
#define RED_LED_CLOSE ledcWrite(0, 0)
#define GREEN_LED_OPEN ledcWrite(1, 145)
#define GREEN_LED_CLOSE ledcWrite(1, 0)*/
#define RED_LED_OPEN digitalWrite(LED_RED_PIN, HIGH)
#define RED_LED_CLOSE  digitalWrite(LED_RED_PIN, LOW)
#define GREEN_LED_OPEN  digitalWrite(LED_GREEN_PIN, HIGH)
#define GREEN_LED_CLOSE  digitalWrite(LED_GREEN_PIN, LOW)

void initGPIO()
{
    //初始化LED灯
    /*ledcSetup(0, 5000, 8);
    ledcAttachPin(LED_RED_PIN, 0);
    ledcSetup(1, 5000, 8);
    ledcAttachPin(LED_GREEN_PIN, 1);*/

    pinMode(LED_RED_PIN, OUTPUT);
    pinMode(LED_GREEN_PIN, OUTPUT);

    /*ledc_timer_config_t ledc_timer;
    ledc_timer.duty_resolution = LEDC_TIMER_13_BIT;
    ledc_timer.freq_hz = 5000;
    ledc_timer.speed_mode = LEDC_LOW_SPEED_MODE;
    ledc_timer.timer_num = LEDC_TIMER_0;

    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel_red;
    ledc_channel_red.channel = LEDC_CHANNEL_0;
    ledc_channel_red.duty = 0;
    ledc_channel_red.gpio_num = LED_RED_PIN;
    ledc_channel_red.speed_mode = LEDC_LOW_SPEED_MODE;
    ledc_channel_red.hpoint = 0;
    ledc_channel_red.timer_sel = LEDC_TIMER_0;

    ledc_channel_config_t ledc_channel_green;
    ledc_channel_green.channel = LEDC_CHANNEL_1;
    ledc_channel_green.duty = 0;
    ledc_channel_green.gpio_num = LED_GREEN_PIN;
    ledc_channel_green.speed_mode = LEDC_LOW_SPEED_MODE;
    ledc_channel_green.hpoint = 0;
    ledc_channel_green.timer_sel = LEDC_TIMER_0;

    ledc_channel_config(&ledc_channel_red);
    ledc_channel_config(&ledc_channel_green);

    ledc_fade_func_install(0);

    ledc_set_fade_with_time(ledc_channel_red.speed_mode, ledc_channel_red.channel, 2000, 3000);
    ledc_fade_start(ledc_channel_red.speed_mode, ledc_channel_red.channel,LEDC_FADE_NO_WAIT);*/

    //切断墨水屏驱动供电
    pinMode(EPD_POWER, OUTPUT);
    digitalWrite(EPD_POWER, 1);

    //初始化墨水屏驱动SPI
    pinMode(BUSY_Pin, INPUT);
    pinMode(RES_Pin, OUTPUT);
    pinMode(DC_Pin, OUTPUT);
    pinMode(CS_Pin, OUTPUT);
    pinMode(SCK_Pin, OUTPUT);
    pinMode(SDI_Pin, OUTPUT);

    //初始化重置按钮
    pinMode(RESETKEY_PIN, INPUT | PULLUP);
    attachInterrupt(RESETKEY_PIN, keyUp, RISING);

    pinMode(RESETKEY_PIN, INPUT);

    //初始化ADC
    pinMode(ADC_PIN, INPUT | ANALOG);
}

void refreshLED(void *Parameter)
{
    //0=红灯，1=绿灯，2=红灯闪烁，3=绿灯闪烁，4=红绿全亮，5=红绿全熄，6=红绿交替闪烁
    while (1)
    {
        if (LED_STATE == 0)
        {
            RED_LED_OPEN;
            GREEN_LED_CLOSE;
        }
        else if (LED_STATE == 1)
        {
            RED_LED_CLOSE;
            GREEN_LED_OPEN;
        }
        else if (LED_STATE == 2)
        {
            if (LED_FLASH_FLAG == 0)
            {
                RED_LED_CLOSE;
                GREEN_LED_CLOSE;
                LED_FLASH_FLAG = 1;
            }
            else if (LED_FLASH_FLAG == 1)
            {
                RED_LED_OPEN;
                GREEN_LED_CLOSE;
                LED_FLASH_FLAG = 0;
            }
        }
        else if (LED_STATE == 3)
        {
            if (LED_FLASH_FLAG == 0)
            {
                RED_LED_CLOSE;
                GREEN_LED_CLOSE;
                LED_FLASH_FLAG = 1;
            }
            else if (LED_FLASH_FLAG == 1)
            {
                RED_LED_CLOSE;
                GREEN_LED_OPEN;
                LED_FLASH_FLAG = 0;
            }
        }
        else if (LED_STATE == 4)
        {
            RED_LED_OPEN;
            GREEN_LED_OPEN;
        }
        else if (LED_STATE == 5)
        {
            RED_LED_CLOSE;
            GREEN_LED_CLOSE;
        }
        else if (LED_STATE == 6)
        {
            if (LED_FLASH_FLAG == 0)
            {
                RED_LED_OPEN;
                GREEN_LED_CLOSE;
                LED_FLASH_FLAG = 1;
            }
            else if (LED_FLASH_FLAG == 1)
            {
                RED_LED_CLOSE;
                GREEN_LED_OPEN;
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
        if (currentState != 2 && KEY_FLAG == 1)
        {
            deviceSetup();
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

void connectWifi()
{
    if (WiFi.isConnected())
        return;
    Serial.println();
    Serial.print("Try to connection wifi...");

    long startConnectTime = millis();

    WiFi.mode(WIFI_STA);
    WiFi.begin(PrefSSID.c_str(), PrefPassword.c_str());

    while (WiFi.status() != WL_CONNECTED)
    {
        if (millis() - startConnectTime > 12 * 1000)
        {
            Serial.println("Connect wifi timeout");
            setCurrentState(1);
            return;
        }
        delay(500);
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

void deviceSetup()
{
    setCurrentState(2);
    Serial.println("reset options");
    if (WiFi.status() == WL_CONNECTED)
    {
        WiFi.disconnect();
    }

    Preferences prefs;
    prefs.begin("options", false);
    prefs.clear();
    prefs.end();
    hasDeviceBinded = false;

    //DEBUG
    /*prefs.begin("options", false);
    prefs.putString("ssid", "azkiki");
    prefs.putString("password", "azkiki123.");
    prefs.putString("BindingID", "fec6273610644fd7bbd6662683efa2b2");
    prefs.end();
    hasDeviceBinded = true;
    ESP.restart();*/

    WiFi.mode(WIFI_AP_STA);
    WiFi.beginSmartConfig();
    //Wait for SmartConfig packet from mobile
    Serial.println("Waiting for SmartConfig.");
    unsigned long smartConfigStartTime = millis();
    while (!WiFi.smartConfigDone())
    {
        if (millis() - smartConfigStartTime > 2 * 60 * 1000)
        {
            WiFi.stopSmartConfig();
            setCurrentState(1);
            Serial.println("SmartConfig timeout");
            return;
        }
        delay(500);
    }
    smartConfigStartTime = millis();
    while (WiFi.status() != WL_CONNECTED)
    {
        if (millis() - smartConfigStartTime > 30 * 1000)
        {
            setCurrentState(1);
            Serial.println("SmartConfig connect wifi timeout");
            return;
        }
        delay(500);
    }

    Serial.println("SmartConfig success");

    PrefSSID = WiFi.SSID();
    PrefPassword = WiFi.psk();

    Serial.println("SSID=" + PrefSSID);
    Serial.println("Password=" + PrefPassword);
    Serial.println("SmartConfig done");
    delay(10);

    smartConfigStartTime = millis();
    BindingID = "";
    Serial.println("wait bindingID");
    httpserver.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("http req");
        request->send(200, "application/json", "{\"code\":0}");
    });
    httpserver.on(CurrentListenBindingIDPath, HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("http req bind");
        if (request->hasParam("msg"))
        {
            String msg;
            msg = request->getParam("msg")->value();
            Serial.printf("Received BindID=%s\n", BindingID.c_str());
            request->send(200, "application/json", "{\"code\":0}");
            BindingID = msg;
        }
        else
        {
            request->send(200, "application/json", "{\"code\":25,\"error\":\"提交绑定参数错误\"}");
        }
        delay(10);
    });
    httpserver.begin();
    while (BindingID != "")
    {
        if (millis() - smartConfigStartTime > 12 * 1000)
        {
            setCurrentState(1);
            Serial.println("wait bindingID timeout");
            httpserver.end();
            return;
        }
        delay(500);
    }
    httpserver.end();

    //Preferences prefs;
    prefs.begin("options", false);
    prefs.putString("ssid", WiFi.SSID());
    prefs.putString("password", WiFi.psk());
    prefs.putString("BindingID", BindingID);
    prefs.end();
    httpserver.end();
    Serial.println("deviceSetup done! restart esp");
    ESP.restart();
}

String GetCurrentDeviceStatus()
{
    String result;
    DynamicJsonDocument doc(1024);
    doc["power"] = "1";
    serializeJson(doc, result);
    return result;
}

void ping()
{
    int salt = rand();
    DynamicJsonDocument doc(1024);
    doc["Salt"] = salt;
    doc["BindingID"] = CurrentBindingID;
    doc["Status"] = GetCurrentDeviceStatus();
    doc["DeviceID"] = CurrentDeviceID;
    doc["FirmwareVersion"] = FIREWARE_VERSION;
    String request = "";
    serializeJson(doc, request);
    Serial.printf("Ping %s\n", request.c_str());
    String response = webApi.Ping(request);
    if (response != "")
    {
        Serial.printf("Ping Result %s\n", response.c_str());
        deserializeJson(doc, response);
        if (doc["Salt"] == salt)
        {
            if (doc["Code"] == 0)
            {
                pingErrorCount = 0;
                if (doc["Command"] != "")
                {
                    setCurrentState(4);
                    handleCommand(doc["Command"].as<String>());
                    setCurrentState(3);
                }
            }
            else
            {
                pingErrorCount++;
            }
        }
    }
    else
    {
        pingErrorCount++;
        Serial.println("Ping Return Empty");
    }
    doc.clear();
}

void deviceSleep()
{
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);

    Serial.printf("light sleep start :%ld ms\n", sleepTime);
    Serial.printf("this time wakeup length :%ld ms\n", millis() - LastSleepTime - sleepTime);
    Serial.printf("current state :%d \n", currentState);
    Serial.printf("current led state :%d \n", LED_STATE);
    Serial.printf("current free memory :%d bytes \n", ESP.getFreeHeap());
    delay(500);
    LastSleepTime = millis();
    esp_sleep_enable_timer_wakeup(sleepTime * 1000);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    esp_light_sleep_start();
    Serial.println("light sleep stop");
}

void setCurrentState(int state)
{
    currentState = state;
    if (currentState == 0)
    {
        LED_STATE = 6;
    }
    else if (currentState == 1)
    {
        LED_STATE = 0;
    }
    else if (currentState == 2)
    {
        LED_STATE = 2;
    }
    else if (currentState == 3)
    {
        LED_STATE = 1;
    }
    else if (currentState == 4)
    {
        LED_STATE = 3;
    }
}

void setBusy(bool busy)
{
    int salt = rand();
    DynamicJsonDocument doc(1024);
    doc["Salt"] = salt;
    doc["BindingID"] = CurrentBindingID;
    doc["DeviceID"] = CurrentDeviceID;
    doc["Busy"] = busy ? "1" : "0";
    String request = "";
    serializeJson(doc, request);
    Serial.printf("Ping %s\n", request.c_str());
    webApi.Ping(request);
}

void handleCommand(String command)
{
    Serial.printf("handleCommand:%s\n", command.c_str());
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, command);
    String type = doc["Type"].as<String>();
    if (type == "ota")
    {
        setBusy(true);
        OTA ota(&webApi);
        ota.execOTA();
    }
    else if (type == "setDisplay")
    {
        setBusy(true);
        String url = doc["url"].as<String>();
        WiFiClient *streamptr = webApi.DownloadDisplayData(url);

        digitalWrite(EPD_POWER, 0); //EPD PowerUp
        EPD_init();                 //EPD init
        PIC_display_start();
        int pos = 0;
        int length = 0;
        uint8_t buf[1024];
        while (1)
        {
            length = streamptr->read(buf, 1024);
            if (length <= 0)
                break;
            PIC_display_part(buf, pos, length);
            pos += length;
        }
        PIC_display_end();
        EPD_refresh();              //EPD_refresh
        EPD_sleep();                //EPD_sleep,Sleep instruction is necessary, please do not delete!!!
        digitalWrite(EPD_POWER, 1); //EPD PowerDown
        setBusy(false);
    }
    else if (type == "clearDisplay")
    {
        setBusy(true);
        digitalWrite(EPD_POWER, 0); //EPD PowerUp
        EPD_init();                 //EPD init
        PIC_display_Clean();
        EPD_refresh();              //EPD_refresh
        EPD_sleep();                //EPD_sleep,Sleep instruction is necessary, please do not delete!!!
        digitalWrite(EPD_POWER, 1); //EPD PowerDown
        setBusy(false);
    }
    else
    {
        Serial.printf("Unkown command type:%s \n", type.c_str());
    }
}