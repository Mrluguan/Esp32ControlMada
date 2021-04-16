#include "WiFi.h"

//检查更新地址
#define check_version_url "https://iot.dotsourceit.com/IoTDevice/GetLatestVersion"
//下载更新地址
#define download_latest_firmware_url "https://iot.dotsourceit.com/IoTDevice/DownloadLatestOTAFirmwareBin"
//节能模式HttpPing地址
#define http_ping_url "https://iot.dotsourceit.com/IoTDevice/Ping"

class WebAPI
{
private:
    WiFiClient *_wifiClient;

public:
    WebAPI(WiFiClient *wifiClient);
    ~WebAPI();
    String Ping(String request);
    WiFiClient* DownloadLatestFirmware(int &contentLength); 
    int GetLatestVersion();
};
