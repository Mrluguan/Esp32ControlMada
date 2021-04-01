#include "OTA.h"
#include <Update.h>

OTA::OTA(WebAPI *webAPI)
{
    _webAPI = webAPI;
}

OTA::~OTA()
{
}

void OTA::execOTA()
{
    Serial.println("exec OTA");
    Serial.println("check version");
    int version = _webAPI->GetLatestVersion();
    Serial.print("Current version:");
    Serial.println(firmwareVersion);
    Serial.print("Latest version:");
    Serial.println(version);
    if (version > 0 && version > firmwareVersion)
    {
        Serial.println("Has update.Start OTA");
        _startOTA();
    }
}

void OTA::_startOTA()
{
    int contentLength = 0;
    WiFiClient stream = _webAPI->DownloadLatestFirmware(contentLength);
    if (&stream == 0)
    {
        Serial.println("DownloadLatestFirmware faild");
        return;
    }
    if (contentLength == 0)
        contentLength = 271632;
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
