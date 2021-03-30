#include <map>
#include <cstring>
#include "Arduino.h"

struct ValueItem
{
    unsigned char *item;
    int len;
};

struct OutputPayload
{
    unsigned char *data;
    int length;

    OutputPayload()
    {
        //Serial.println("OutputPayload Create");
    }
    ~OutputPayload()
    {
        delete data;
        //Serial.println("OutputPayload Destory");
    }
};

class DotsMqttMessage
{
public:
    DotsMqttMessage();
    ~DotsMqttMessage();

    void FromPayload(unsigned char *data);
    OutputPayload ToPayload();
    ValueItem GetValue(std::string key);
    std::string GetString(std::string key);
    int GetInt32(std::string key);
    long GetInt64(std::string key);
    double GetDouble(std::string key);

    void SetString(std::string key, std::string value);
    void SetInt32(std::string key, int value);
    void SetInt64(std::string key, long value);
    void SetDouble(std::string key, double value);
    void SetBoolean(std::string key, bool value);
    void SetByteArray(std::string key, unsigned char *value, int length);
    void SetValue(std::string key, ValueItem value);

private:
    std::map<std::string, ValueItem> innermap;
};