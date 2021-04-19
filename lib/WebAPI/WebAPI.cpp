#include "WebAPI.h"
#include "HTTPClient.h"
#include "stdlib.h"
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "OTA.h"

WebAPI::WebAPI(String deviceID, String bindingID)
{
    _deviceID = deviceID;
    _bindingID = bindingID;
}

WebAPI::~WebAPI()
{
}

String WebAPI::Ping(String status)
{
    try
    {
        int salt = rand();
        DynamicJsonDocument doc(1024);
        doc["Salt"] = salt;
        doc["BindingID"] = _bindingID;
        doc["Status"] = status;
        doc["DeviceID"] = _deviceID;
        doc["FirmwareVersion"] = FIREWARE_VERSION;
        String payload;
        serializeJson(doc,payload);
        
        HTTPClient client;
        client.begin(http_ping_url);
        client.addHeader("Content-Type", "application/json");
        client.addHeader("Cache-Control", "no-cache");
        String result = "";
        int sc = client.POST(payload);
        if (sc == 200)
        {
            result = client.getString();
        }
        Serial.printf("HTTP Status code = %d\n", sc);
        client.end();
        return result;
    }
    catch (const std::exception &e)
    {
        Serial.printf("HTTP Error");
        return "";
    }
}

int WebAPI::GetLatestVersion()
{
    try
    {
        HTTPClient client;
        client.begin(check_version_url);
        int sc = client.GET();
        client.end();
        if (sc == 200)
        {
            int version = atoi(client.getString().c_str());
            return version;
        }
        return 0;
    }
    catch (const std::exception &e)
    {
        Serial.println("GetLatestVersion Error");
        return 0;
    }
}

WiFiClient *WebAPI::DownloadLatestFirmware(int &contentLength, HTTPClient &client)
{
    try
    {
        client.begin(download_latest_firmware_url);
        int sc = client.GET();
        if (sc == 200)
        {
            String length = client.header("Content-Length");
            Serial.print("Content-Length");
            Serial.println(length);
            contentLength = atoi(length.c_str());
            return client.getStreamPtr();
        }
        return NULL;
    }
    catch (const std::exception &e)
    {
        Serial.println("Time Error");
        return NULL;
    }
}

WiFiClient *WebAPI::DownloadDisplayData(String url, HTTPClient &client)
{
    try
    {
        client.begin(url);
        int sc = client.GET();
        if (sc == 200)
        {
            return client.getStreamPtr();
        }
        return NULL;
    }
    catch (const std::exception &e)
    {
        Serial.println("Time Error");
        return NULL;
    }
}

void WebAPI::SetBusyStatus(bool busy)
{
    try
    {
        int salt = rand();
        DynamicJsonDocument doc(1024);
        doc["Salt"] = salt;
        doc["BindingID"] = _bindingID;
        doc["Busy"] = busy;
        String payload;
        serializeJson(doc,payload);
        HTTPClient client;
        client.begin(http_set_busy_url);
        client.addHeader("Content-Type", "application/json");
        client.addHeader("Cache-Control", "no-cache");
        int sc = client.POST(payload);
        Serial.printf("HTTP Status code = %d\n", sc);
        client.end();
    }
    catch (const std::exception &e)
    {
        Serial.printf("HTTP Error");
    }
}

void WebAPI::CommandHandleResultCallback(String result)
{
    try
    {
        int salt = rand();
        DynamicJsonDocument doc(1024);
        doc["Salt"] = salt;
        doc["BindingID"] = _bindingID;
        doc["Busy"] = busy;
        String payload;
        serializeJson(doc,payload);
        HTTPClient client;
        client.begin(http_command_callback_url);
        client.addHeader("Content-Type", "application/json");
        client.addHeader("Cache-Control", "no-cache");
        int sc = client.POST(payload);
        Serial.printf("HTTP Status code = %d\n", sc);
        client.end();
    }
    catch (const std::exception &e)
    {
        Serial.printf("HTTP Error");
    }
}
