#include <WiFiClient.h>
#include <HTTPClient.h>

#ifndef _WEBAPI_H_
#define _WEBAPI_H_

//检查更新地址
#define check_version_url "https://iot.dotsourceit.com/IoTDevice/GetLatestVersion"
//下载更新地址
#define download_latest_firmware_url "https://iot.dotsourceit.com/IoTDevice/DownloadLatestOTAFirmwareBin"
//HttpPing地址
#define http_ping_url "https://iot.dotsourceit.com/HTTPGateway/Ping"
//SetBusyStatus地址
#define http_set_busy_url "https://iot.dotsourceit.com/HTTPGateway/SetBusyStatus"
//CommandCallback地址
#define http_command_callback_url "https://iot.dotsourceit.com/HTTPGateway/CommandCallback"

class WebAPI
{
private:
    String _deviceID;
    String _bindingID;

public:
    WebAPI(String deviceID,String bindingID);
    ~WebAPI();
    String Ping(String request);
    WiFiClient *DownloadLatestFirmware(int &contentLength,HTTPClient& client);
    int GetLatestVersion();
    WiFiClient *DownloadDisplayData(String url,HTTPClient& client);
    void SetBusyStatus(bool busy);
    void CommandHandleResultCallback(String result);
};

#endif