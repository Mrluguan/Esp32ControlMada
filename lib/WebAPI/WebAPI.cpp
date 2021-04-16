#include "WebAPI.h"
#include "WiFi.h"
#include "HTTPClient.h"
#include "stdlib.h"

WebAPI::WebAPI(WiFiClient *wifiClient)
{
    _wifiClient = wifiClient;
}

WebAPI::~WebAPI()
{
}

String WebAPI::Ping(String payload)
{
    try
    {
        HTTPClient client;
        client.begin(*_wifiClient, http_ping_url);
        int sc = client.POST(payload);
        if (sc == 200)
        {
            return client.getString();
        }
        Serial.printf("HTTP Status code = %d\n",sc);
        return "";
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
        client.begin(*_wifiClient, check_version_url);
        int sc = client.GET();
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

WiFiClient *WebAPI::DownloadLatestFirmware(int &contentLength)
{
    try
    {
        HTTPClient client;
        client.begin(*_wifiClient, download_latest_firmware_url);
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