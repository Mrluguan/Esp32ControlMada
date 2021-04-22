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

#define udp_remote_address "iot.dotsourceit.com"
#define udp_remote_port 24516
#define udp_ping_action "Ping"
#define udp_set_busy_action "SetBusyStatus"
#define udp_command_callback_action "CommandCallback"

class WebAPI
{
private:
    String _deviceID;
    String _bindingID;
    void AESEncryption(unsigned char *source, int length, unsigned char *output);
    void AESDencryption(unsigned char *source, int length, unsigned char *output);

public:
    WebAPI(String deviceID, String bindingID);
    ~WebAPI();
    String Ping(String status);
    String UdpPing(String status);
    WiFiClient *DownloadLatestFirmware(int &contentLength, HTTPClient &client);
    int GetLatestVersion();
    WiFiClient *DownloadDisplayData(String url, HTTPClient &client);
    void SetBusyStatus(bool busy);
    void CommandHandleResultCallback(String commandID, String result, bool CancelBusyStatus);
};

#endif