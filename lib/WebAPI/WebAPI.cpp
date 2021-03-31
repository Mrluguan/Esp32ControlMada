#include "WebAPI.h"
#include "WiFi.h"
#include "HTTPClient.h"
#include "staticconfig.h"
#include "stdlib.h"

WebAPI::WebAPI(WiFiClient *wifiClient)
{
    _wifiClient = wifiClient;
}

WebAPI::~WebAPI()
{
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

long WebAPI::Time()
{
    try
    {
        HTTPClient client;
        client.begin(*_wifiClient, check_version_url);
        int sc = client.GET();
        if (sc == 200)
        {
            char *pos;
            long version = strtol(client.getString().c_str(), &pos, 10);
            return version;
        }
        return 0;
    }
    catch (const std::exception &e)
    {
        Serial.println("Time Error");
        return 0;
    }
}

WiFiClient* WebAPI::DownloadLatestFirmware(int *contentLength)
{
    try
    {
        HTTPClient client;
        client.begin(*_wifiClient, download_latest_firmware_url);
        int sc = client.GET();
        if (sc == 200)
        {
            String length = client.header("Content-Length");
            contentLength
            return client.getStreamPtr();
        }
        return 0;
    }
    catch (const std::exception &e)
    {
        Serial.println("Time Error");
        return 0;
    }
}