#include "WebAPI.h"

//当前内部固件程序版本号
#define FIREWARE_VERSION 12

class OTA
{
private:
    WebAPI *_webAPI;
    void _startOTA();

public:
    OTA(WebAPI *webAPI);
    ~OTA();
    void execOTA();
};

