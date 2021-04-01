#include "WebAPI.h"

//当前内部固件程序版本号
#define firmwareVersion 12

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

