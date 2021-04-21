#include "WebAPI.h"
#include "HTTPClient.h"
#include "stdlib.h"
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "OTA.h"
#include <WiFiUdp.h>
#include "mbedtls/aes.h"

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
        serializeJson(doc, payload);

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
        Serial.printf("Ping HTTP Status code = %d\n", sc);
        client.end();
        return result;
    }
    catch (const std::exception &e)
    {
        Serial.printf("HTTP Error");
        return "";
    }
}

void WebAPI::AESEncryption(unsigned char *source, int length, unsigned char *output)
{
    if (length % 16 != 0)
    {
        throw "length not right";
    }
    Serial.printf("Aes length = %d\n",length);
    long startAesTime = millis();
    unsigned char key[16] = {202, 43, 108, 55, 135, 138, 172, 74, 128, 210, 90, 233, 120, 162, 121, 129};
    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);
    int keybits = 256;
    int ret = 0;
    ret = mbedtls_aes_setkey_enc(&ctx, key, keybits);

    int pos = 0;
    while (pos < length)
    {
        unsigned char inbuf[16];
        unsigned char outbuf[16];
        for (int i = 0; i < 16; i++)
        {
            inbuf[i] = source[pos + i];
        }
        ret = mbedtls_aes_crypt_ecb(&ctx, MBEDTLS_AES_ENCRYPT, inbuf, outbuf);
        if (ret != 0)
        {
            throw "aes block error";
        }
        for (int i = 0; i < 16; i++)
        {
            output[pos + i] = outbuf[i];
        }
        pos += 16;
    }
    Serial.printf("aes use %d ms.\n", millis() - startAesTime);
}

String WebAPI::UdpPing(String status)
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
        serializeJson(doc, payload);

        int length = payload.length();
        if(length%16!=0)
        {
            for(int i=0;i<(16 - (length % 16));i++)
            {
                payload+=" ";
            }
            length = payload.length();
        }
        unsigned char buf[length];
        payload.getBytes(buf, length);

        unsigned char encryptBuf[length];
        AESEncryption(buf, length, encryptBuf);

        WiFiUDP udp;
        udp.begin(12004);
        udp.beginPacket(udp_remote_address, udp_remote_port);
        udp.write(0);
        udp.write(1);
        udp.write(2);
        udp.write(3);
        udp.write(encryptBuf, length + 4);
        udp.endPacket();

        int timeout = 3000;
        int waitTime = 0;
        while (!udp.parsePacket())
        {
            delay(100);
            waitTime += 100;
            if (waitTime >= timeout)
                return "";
        }
        unsigned char receiveBuf[1400];
        int len = udp.read(receiveBuf, 1400);
        String result;
        for (int i = 0; i < len; i++)
            result += receiveBuf[i];
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
        doc["FirmwareVersion"] = FIREWARE_VERSION;
        doc["DeviceID"] = _deviceID;
        String payload;
        serializeJson(doc, payload);
        HTTPClient client;
        client.begin(http_set_busy_url);
        client.addHeader("Content-Type", "application/json");
        client.addHeader("Cache-Control", "no-cache");
        int sc = client.POST(payload);
        Serial.printf("SetBusyStatus HTTP Status code = %d\n", sc);
        client.end();
    }
    catch (const std::exception &e)
    {
        Serial.printf("HTTP Error");
    }
}

void WebAPI::CommandHandleResultCallback(String commandID, String result, bool CancelBusyStatus)
{
    try
    {
        int salt = rand();
        DynamicJsonDocument doc(1024);
        doc["Salt"] = salt;
        doc["BindingID"] = _bindingID;
        doc["CommandID"] = commandID;
        doc["FirmwareVersion"] = FIREWARE_VERSION;
        doc["DeviceID"] = _deviceID;
        doc["Result"] = result;
        doc["CancelBusyStatus"] = CancelBusyStatus;
        String payload;
        serializeJson(doc, payload);
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
