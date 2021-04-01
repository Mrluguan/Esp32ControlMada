#include "WebAPI.h"
#include "staticconfig.h"
#include <Update.h>

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

OTA::OTA(WebAPI *webAPI)
{
    _webAPI = webAPI;
}

OTA::~OTA()
{
}
