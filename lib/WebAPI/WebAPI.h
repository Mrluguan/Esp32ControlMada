#include <WiFiClient.h>
#include <HTTPClient.h>

#ifndef _WEBAPI_H_
#define _WEBAPI_H_

//当前内部固件程序版本号
#define FIREWARE_VERSION 202104255
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
    WiFiClient *DownloadFirmware(String url,int &contentLength, HTTPClient &client);
    WiFiClient *DownloadDisplayData(String url, HTTPClient &client);
    void SetBusyStatus(bool busy);
    void CommandHandleResultCallback(String commandID, String result, bool CancelBusyStatus);
};

#endif