#include "WiFi.h"

//检查更新地址
//#define check_version_url "https://iot.dotsourceit.com/IoTDevice/GetLatestVersion"
#define check_version_url "http://192.168.31.154:8001/version.txt"
//下载更新地址
//#define download_latest_firmware_url "https://iot.dotsourceit.com/IoTDevice/DownloadLatestOTAFirmwareBin"
#define download_latest_firmware_url "http://192.168.31.154:8001/firmware.bin"
//当前时区偏移量
#define timezone_offset 8
//节能模式HttpPing地址
#define http_ping_url "https://iot.dotsourceit.com/IoTDevice/Ping"
//获取当前网络时间戳地址
#define get_current_timespan_url "https://iot.dotsourceit.com/IoTDevice/Time"

class WebAPI
{
private:
    WiFiClient *_wifiClient;

public:
    WebAPI(WiFiClient *wifiClient);
    ~WebAPI();
    WiFiClient DownloadLatestFirmware(int &contentLength); 
    int GetLatestVersion();
    long Time();
};
