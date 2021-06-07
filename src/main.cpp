#include <Preferences.h>
#include <WiFi.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "staticconfig.h"
#include <ESPAsyncWebServer.h>
#include <EPD_display.h>
#include <CRC.h>
#include "Arduino.h"
#include "time.h"
#include <esp_pm.h>
#include <driver/ledc.h>
#include <ArduinoJson.h>
#include "WebAPI.h"
#include "mbedtls/aes.h"
#include "Update.h"
#include "esp32/ulp.h"
#include "driver/rtc_io.h"
#include "iBeacon.h"

/*#define RED_LED_OPEN ledcWrite(0, 200)
#define RED_LED_CLOSE ledcWrite(0, 0)
#define GREEN_LED_OPEN ledcWrite(1, 145)
#define GREEN_LED_CLOSE ledcWrite(1, 0)*/
#define RED_LED_OPEN digitalWrite(LED_RED_PIN, HIGH)
#define RED_LED_CLOSE digitalWrite(LED_RED_PIN, LOW)
#define GREEN_LED_OPEN digitalWrite(LED_GREEN_PIN, HIGH)
#define GREEN_LED_CLOSE digitalWrite(LED_GREEN_PIN, LOW)

char *CurrentBindingID = new char[32 + 1];
char *CurrentListenBindingIDPath = new char[10 + 8 + 1 + 1];
char *CurrentDeviceID = new char[10 + 8 + 1];
char *CurrentDeviceIDWithoutCompany = new char[8 + 8 + 1];

AsyncWebServer httpserver(8080);
WebAPI *webApi;

int LED_STATE = 4; //0=红灯，1=绿灯，2=红灯闪烁，3=绿灯闪烁，4=红绿全亮，5=红绿全熄，6=红绿交替闪烁
int LED_FLASH_FLAG = 0;

int currentState = 0; //0=刚开机，1=无法连接，2=正在配网，3=正常工作，4=繁忙

bool wifiConnecting = false;                               //是否正在尝试连接wifi
wifi_err_reason_t wifiErrReason = WIFI_REASON_UNSPECIFIED; //上一次连接wifi错误的原因

bool hasDeviceBinded = false; //设备是否已经绑定

int notAPFoundTimes = 0; //尝试连接，找不到ap的次数

int pingErrorCount = 0; //ping失败次数

long sleepTimeSec = 5;  //睡眠时长
long LastSleepTime = 0; //上一次睡眠时间

String PrefSSID = "";     //SSID
String PrefPassword = ""; //Password
String BindingID = "";    //绑定ID

float LatestVoltage = 3; //上一次测得的电压
int LatestWifiRSSI = 3;  //上一次测得的信号强度

long LatestPingReturnTimeInfo = 0; //上一次Ping响应返回的时间戳
long LatestPingReturnTime = 0;     //上一次Ping的响应时间

//重置设备
void resetDevice();
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
//ping
void ping();
//获取当前设备状态字符串
String GetCurrentDeviceStatus();
//设备睡眠
void deviceSleep();
//设置当前状态0=刚开机，1=无法连接，2=正在配网，3=正常工作，4=繁忙
void setCurrentState(int state);
//write
void writeStreamToDisplay(Stream &stream);
//处理命令
void handleCommand(String command);
//OTA
void httpOTA(Stream &stream, int contentLength);
//命令处理超时看门狗
void handleCommandWatchDog();
//获得当前时间
bool getCurrentDateTime(tm *ltm);

void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info)
{
    switch (event)
    {
    case SYSTEM_EVENT_STA_DISCONNECTED:
        wifiErrReason = (wifi_err_reason_t)info.disconnected.reason;
        WiFi.disconnect();
        wifiConnecting = false;
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        wifiErrReason = WIFI_REASON_UNSPECIFIED;
        wifiConnecting = false;
        notAPFoundTimes = 0;
        break;
    default:
        break;
    }
}

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
    setCpuFrequencyMhz(80);
    setCurrentState(0);

    Serial.printf("current firmware version : %d\n", FIREWARE_VERSION);
    uint32_t flash_size = ESP.getFlashChipSize();
    Serial.printf("flash size = %dMB\n", flash_size / 1024 / 1024);
    //初始化GPIO
    initGPIO();
    //初始化系统基础配置
    initParameters();

    //LED刷新任务
    xTaskCreatePinnedToCore((TaskFunction_t)refreshLED, "refreshLED", 1024, (void *)NULL, (UBaseType_t)2, (TaskHandle_t *)NULL, (BaseType_t)tskNO_AFFINITY);

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
        webApi = new WebAPI(CurrentDeviceID, CurrentBindingID);
    }
    //按钮监控任务
    xTaskCreatePinnedToCore((TaskFunction_t)keyMonitor, "keyMonitor", 4096, (void *)NULL, (UBaseType_t)2, (TaskHandle_t *)NULL, (BaseType_t)tskNO_AFFINITY);
}

void loop()
{
    //return;
    if (millis() > 40ul * 24ul * 60ul * 60ul * 1000ul)
    {
        ESP.restart();
        return;
    }
    if (currentState == 2 || digitalRead(RESETKEY_PIN) == 0)
        return;
    if (hasDeviceBinded)
    {
        connectWifi();
        if (WiFi.isConnected())
        {
            long startPingTime = millis();
            ping();
            Serial.printf("ping use %ld ms.\n", millis() - startPingTime);
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
    if (currentState != 2)
        deviceSleep();
    delay(100);
}

void initParameters()
{
    uint64_t chipid = ESP.getEfuseMac(); //The chip ID is essentially its MAC address(length: 6 bytes).
    unsigned char value[sizeof(chipid)];
    memcpy(value, &chipid, sizeof(chipid));
    Serial.print("MAC = ");
    for (int i = 0; i < 6; i++)
    {
        Serial.printf("%x", value[i]);
    }
    Serial.println();

    uint crcRes = CRC32(value, sizeof(chipid));
    strcpy(CurrentDeviceIDWithoutCompany, DeviceCategory);
    strcat(CurrentDeviceIDWithoutCompany, DeviceModelNo);
    sprintf(CurrentDeviceIDWithoutCompany + 8, "%-4X", crcRes);
    Serial.printf("CurrentDeviceIDWithoutCompany = %s\n", CurrentDeviceIDWithoutCompany);

    strcpy(CurrentDeviceID, CompanyNo);
    strcat(CurrentDeviceID, CurrentDeviceIDWithoutCompany);

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

void initGPIO()
{
    //初始化LED灯
    pinMode(LED_RED_PIN, OUTPUT);
    pinMode(LED_GREEN_PIN, OUTPUT);

    //切断墨水屏驱动供电
    //pinMode(EPD_POWER, OUTPUT);
    //digitalWrite(EPD_POWER, HIGH);

    //初始化墨水屏驱动SPI
    pinMode(BUSY_Pin, INPUT);
    pinMode(RES_Pin, OUTPUT);
    pinMode(DC_Pin, OUTPUT);
    pinMode(CS_Pin, OUTPUT);
    pinMode(SCK_Pin, OUTPUT);
    pinMode(SDI_Pin, OUTPUT);

    //初始化重置按钮
    pinMode(RESETKEY_PIN, INPUT | PULLUP);

    //attachInterrupt(RESETKEY_PIN, keyUp, RISING);

    //初始化ADC
    pinMode(ADC_PIN, INPUT | ANALOG);

    pinMode(CHANGE_STATE_PIN, INPUT);
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
        //if reset keydown, green led open
        if (digitalRead(RESETKEY_PIN) == 0 && currentState != 2)
        {
            GREEN_LED_OPEN;
        }
        vTaskDelay(500);
    }
}

void keyMonitor(void *Parameter)
{
    while (1)
    {
        int key_down_c = 0;
        while (digitalRead(RESETKEY_PIN) == 0)
        {
            delay(200);
            key_down_c += 200;
            if (currentState != 2 && key_down_c >= 5000)
            {
                setCurrentState(2);
                Serial.println("reset");
                resetDevice();
            }
        }
        delay(100);
    }
}

void connectWifi()
{
    if (WiFi.isConnected())
        return;
    Serial.println();
    Serial.print("Try to connection wifi...");
    long startConnectTime = millis();

    WiFi.mode(WIFI_STA);
    WiFi.onEvent(onWiFiEvent);
    wifiConnecting = true;
    WiFi.begin(PrefSSID.c_str(), PrefPassword.c_str());
    while (wifiConnecting)
    {
        if (millis() - startConnectTime > 8 * 1000)
        {
            Serial.printf("Connect wifi timeout\n");
            WiFi.disconnect(true);
            setCurrentState(1);
            return;
        }
        delay(100);
    }
    Serial.printf("Connect wifi use %ld ms.\n", millis() - startConnectTime);
    if (WiFi.status() != WL_CONNECTED)
    {
        setCurrentState(1);
    }
}

void resetDevice()
{
    Preferences prefs;
    prefs.begin("options", false);
    prefs.clear();
    prefs.end();
    hasDeviceBinded = false;
    ESP.restart();
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

    //Start Beacon
    StartDeviceIDBeacon(CurrentDeviceIDWithoutCompany);

    WiFi.mode(WIFI_AP_STA);
    WiFi.beginSmartConfig();
    //Wait for SmartConfig packet from mobile
    Serial.println("Waiting for SmartConfig.");
    unsigned long smartConfigStartTime = millis();
    while (!WiFi.smartConfigDone())
    {
        if (millis() - smartConfigStartTime > 3 * 60 * 1000)
        {
            StopBeacon();
            WiFi.stopSmartConfig();
            setCurrentState(1);
            Serial.println("SmartConfig timeout");
            return;
        }
        delay(500);
    }
    StopBeacon();
    smartConfigStartTime = millis();
    while (WiFi.status() != WL_CONNECTED)
    {
        if (millis() - smartConfigStartTime > 2 * 60 * 1000)
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
            delay(1000);
            BindingID = msg;
        }
        else
        {
            request->send(200, "application/json", "{\"code\":25,\"error\":\"提交绑定参数错误\"}");
        }
        delay(10);
    });
    httpserver.begin();
    while (BindingID == "")
    {
        if (millis() - smartConfigStartTime > 20 * 1000)
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
    LatestWifiRSSI = WiFi.RSSI();
    Serial.printf("current wifi rssi :%d\n", LatestWifiRSSI);
    Serial.printf("current state :%d \n", currentState);
    Serial.printf("current led state :%d \n", LED_STATE);
    Serial.printf("current free memory :%d bytes \n", ESP.getFreeHeap());

    Serial.printf("current change state :%d\n", digitalRead(CHANGE_STATE_PIN));

    /*long total = 0;
    Serial.println("Start collect adc");
    for (int i = 0; i < 10; i++)
    {
        total += analogRead(ADC_PIN);
        delay(10);
    }*/
    //float readingADC = total / 10.000;
    float readingADC = analogRead(ADC_PIN);
    Serial.println("Stop collect adc");
    Serial.printf("Current battery adc : %f\n", readingADC);
    float readingVoltage = -0.000000000000016 * pow(readingADC, 4) + 0.000000000118171 * pow(readingADC, 3) - 0.000000301211691 * pow(readingADC, 2) + 0.001109019271794 * readingADC + 0.034143524634089;
    LatestVoltage = readingVoltage * 2;
    Serial.printf("Current battery voltage : %f v\n", LatestVoltage);

    int rssiLevel = 0;
    if (LatestWifiRSSI > -55)
    {
        rssiLevel = 3;
    }
    else if (LatestWifiRSSI > -66)
    {
        rssiLevel = 2;
    }
    else if (LatestWifiRSSI > -77)
    {
        rssiLevel = 1;
    }
    else
    {
        rssiLevel = 0;
    }

    int powerLevel = 0;

    if (LatestVoltage >= 3.7)
    {
        powerLevel = 3;
    }
    else if (LatestVoltage >= 3.55)
    {
        powerLevel = 2;
    }
    else if (LatestVoltage >= 3.40)
    {
        powerLevel = 1;
    }
    else
    {
        powerLevel = 0;
    }

    String result;
    DynamicJsonDocument doc(1024);
    doc["power"] = powerLevel;
    doc["wifi"] = rssiLevel;
    serializeJson(doc, result);
    doc.clear();
    return result;
}

void ping()
{
    String response = webApi->UdpPing(GetCurrentDeviceStatus());
    if (response != "")
    {
        DynamicJsonDocument doc(1024);
        Serial.printf("Ping Result %s\n", response.c_str());
        deserializeJson(doc, response);
        if (doc["Code"] == 0)
        {
            pingErrorCount = 0;
            LatestPingReturnTimeInfo = doc["Time"].as<long>();
            LatestPingReturnTime = millis();
            if (doc["Command"] != "")
            {
                setCurrentState(4);
                //xTaskCreatePinnedToCore((TaskFunction_t)keyMonitor, "handleCommandWatchDog", 2048, (void *)NULL, (UBaseType_t)2, (TaskHandle_t *)NULL, (BaseType_t)tskNO_AFFINITY);
                handleCommand(doc["Command"].as<String>());
                setCurrentState(3);
                delay(500);
            }
        }
        else
        {
            pingErrorCount++;
        }
    }
    else
    {
        pingErrorCount++;
        Serial.println("Ping Return Empty");
    }
}

void deviceSleep()
{
    if (currentState != 2)
    {
        Serial.printf("this time wakeup length :%ld ms\n", millis() - LastSleepTime - sleepTimeSec * 1000);

        tm timeinfo;
        if (!getCurrentDateTime(&timeinfo))
        {
            Serial.println("Failed to obtain time");
            sleepTimeSec = 15;
        }
        else
        {
            Serial.printf("hour = %d\n",timeinfo.tm_hour);
            if (timeinfo.tm_hour >= 23 || timeinfo.tm_hour < 7)
            {
                sleepTimeSec = 30;
            }
            else
            {
                sleepTimeSec = sleepTimeSec + 1;
                if (sleepTimeSec > 15)
                {
                    sleepTimeSec = 8;
                }
            }
        }

        if (WiFi.status() == WL_CONNECTED)
        {
            WiFi.disconnect();
        }
        WiFi.mode(WIFI_OFF);

        bool deepSleep = false;
        if (wifiErrReason == WIFI_REASON_NO_AP_FOUND)
        {
            if (notAPFoundTimes > 3)
            {
                //deepSleep = true;
                sleepTimeSec = 50;
            }
            else
            {
                Serial.println("AP not found ++");
                notAPFoundTimes++;
            }
        }

        LastSleepTime = millis();
        Serial.printf("light sleep start :%ld ms\n", sleepTimeSec * 1000);
        esp_sleep_enable_timer_wakeup(sleepTimeSec * 1000000);
        gpio_wakeup_enable((gpio_num_t)RESETKEY_PIN, GPIO_INTR_LOW_LEVEL);
        esp_sleep_enable_gpio_wakeup();
        delay(100);

        if (deepSleep)
        {
            esp_deep_sleep_start();
        }
        else
        {
            esp_light_sleep_start();
        }

        esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();
        if (wakeup_cause == ESP_SLEEP_WAKEUP_GPIO)
        {
            GREEN_LED_OPEN;
        }
        pinMode(RESETKEY_PIN, INPUT | PULLUP);
        Serial.println("light sleep stop");
    }
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
        LED_STATE = 5;
    }
    else if (currentState == 4)
    {
        LED_STATE = 3;
    }
}

void writeStreamToDisplay(Stream &stream)
{
    if (!stream.available())
    {
        Serial.println("stream isn't available");
        return;
    }
    EPD_init(); //EPD init
    int pos = 0;
    int length = 0;
    uint8_t *buf = new uint8_t[4096];
    while (1)
    {
        length = stream.readBytes(buf, 4096);
        if (length <= 0)
            break;
        Serial.printf("Download pos = %d\n", pos);
        PIC_display_part(buf, pos, length);
        pos += length;
    }
    Serial.println("EPD data write finish");
    EPD_refresh();
    EPD_sleep();
    Serial.println("EPD refresh finish!");
}

void handleCommand(String command)
{
    Serial.printf("handleCommand:%s\n", command.c_str());
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, command);
    String type = doc["Type"].as<String>();
    if (type == "ota")
    {
        webApi->SetBusyStatus(true);
        String url = doc["Command"].as<String>();
        int contentLength;
        HTTPClient client;
        WiFiClient *streamptr = webApi->DownloadFirmware(url, contentLength, client);
        httpOTA(*streamptr, contentLength);
        webApi->CommandHandleResultCallback(doc["CommandID"], "success", true);
    }
    else if (type == "setDisplay")
    {
        webApi->SetBusyStatus(true);
        String url = doc["Command"].as<String>();
        HTTPClient http;
        WiFiClient *streamptr = webApi->DownloadDisplayData(url, http);
        writeStreamToDisplay(*streamptr);
        http.end();
        webApi->CommandHandleResultCallback(doc["CommandID"], "success", true);
    }
    else if (type == "cleanDisplay")
    {
        webApi->SetBusyStatus(true);
        EPD_init(); //EPD init
        PIC_display_Clean();
        EPD_refresh(); //EPD_refresh
        EPD_sleep();   //EPD_sleep,Sleep instruction is necessary, please do not delete!!!
        webApi->CommandHandleResultCallback(doc["CommandID"], "success", true);
    }
    else
    {
        Serial.printf("Unkown command type:%s \n", type.c_str());
    }
}

void httpOTA(Stream &stream, int contentLength)
{
    if (!stream.available())
    {
        Serial.println("stream isn't available");
        return;
    }
    Serial.print("LatestFirmware Size = ");
    Serial.println(contentLength);
    // Check if there is enough to OTA Update
    bool canBegin = Update.begin(contentLength);
    // If yes, begin
    if (canBegin)
    {
        Serial.println("Begin OTA. This may take 2 - 5 mins to complete. Things might be quite for a while.. Patience!");
        // No activity would appear on the Serial monitor
        // So be patient. This may take 2 - 5mins to complete
        size_t written = Update.writeStream(stream);
        if (written == contentLength)
        {
            Serial.println("Written : " + String(written) + " successfully");
        }
        else
        {
            Serial.println("Written only : " + String(written) + "/" + String(contentLength) + ". Retry?");
            // retry??
            // execOTA();
        }
        if (Update.end())
        {
            Serial.println("OTA done!");
            if (Update.isFinished())
            {
                Serial.println("Update successfully completed. Rebooting.");
                ESP.restart();
            }
            else
            {
                Serial.println("Update not finished? Something went wrong!");
            }
        }
        else
        {
            Serial.println("Error Occurred. Error #: " + String(Update.getError()));
        }
    }
    else
    {
        // not enough space to begin OTA
        // Understand the partitions and
        // space availability
        Serial.println("Not enough space to begin OTA");
        stream.flush();
    }
}

void handleCommandWatchDog()
{
    long start = millis();
    while (currentState == 4)
    {
        /* code */
        delay(1000);
        if ((millis() - start) > 90 * 1000)
        {
            Serial.println("handleCommand timeout");
            ESP.restart();
        }
    }
}

bool getCurrentDateTime(tm *ltm)
{
    if (LatestPingReturnTimeInfo == 0)
        return false;
    long now = LatestPingReturnTimeInfo + (long)((millis() - LatestPingReturnTime) / 1000);
    if (TimeZone != 0)
    {
        now += TimeZone * 60 * 60;
    }
    tm *temp = localtime(&now);
    ltm->tm_year = temp->tm_year,
    ltm->tm_mon = temp->tm_mon;
    ltm->tm_mday = temp->tm_mday;
    ltm->tm_hour = temp->tm_hour;
    ltm->tm_min = temp->tm_min;
    ltm->tm_sec = temp->tm_sec;
    Serial.printf("Current time = %d-%d-%d,%d:%d:%d\n", 1900 + ltm->tm_year, 1 + ltm->tm_mon, ltm->tm_mday, ltm->tm_hour, ltm->tm_min, ltm->tm_sec);
    return true;
}
