#include "WiFi.h"

class WebAPI
{
private:
    WiFiClient *_wifiClient;

public:
    WebAPI(WiFiClient *wifiClient);
    ~WebAPI();
    WiFiClient* DownloadLatestFirmware(); 
    int GetLatestVersion();
    long Time();
};
