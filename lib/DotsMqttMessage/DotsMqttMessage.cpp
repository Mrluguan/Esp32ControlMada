#include "DotsMqttMessage.h"
#include <malloc.h>
#include "Arduino.h"

DotsMqttMessage::DotsMqttMessage()
{
    Serial.println("DotsMqttMessage Class Create");
}

DotsMqttMessage::~DotsMqttMessage()
{
    Serial.println("DotsMqttMessage Class Destory");
    try
    {
        std::map<std::string, ValueItem>::iterator iter;
        for (iter = innermap.begin(); iter != innermap.end(); iter++)
        {
            if (iter->second.item != NULL)
                delete iter->second.item;
        }
        innermap.clear();
    }
    catch (int)
    {
    }
}

void DotsMqttMessage::FromPayload(unsigned char *data)
{
    innermap.clear();
    unsigned char count = data[0];
    int pos = 1;
    for (int i = 0; i < count; i++)
    {
        int keyLength;
        memcpy(&keyLength, data + pos, 4);
        pos += 4;

        //char *keyData = new char[keyLength];
        //memcpy(keyData, data + pos, keyLength);
        //std::string key(keyData, keyLength);
        std::string key;
        key.assign((char *)(data + pos), keyLength);
        pos += keyLength;

        int valueLength;
        memcpy(&valueLength, data + pos, 4);
        pos += 4;

        char *valueData = new char[valueLength];
        memcpy(valueData, data + pos, valueLength);
        //unsigned char *valueData = data + pos;
        pos += valueLength;

        ValueItem item;
        item.item = (unsigned char *)valueData;
        item.len = valueLength;

        SetValue(key, item);
    }
}

OutputPayload DotsMqttMessage::ToPayload()
{
    if (innermap.empty())
    {
        throw "empty";
    }
    int size = 1;
    std::map<std::string, ValueItem>::iterator iter;
    for (iter = innermap.begin(); iter != innermap.end(); iter++)
    {
        size += iter->first.length();
        size += iter->second.len;
        size += 8;
    }
    unsigned char *result = new unsigned char[size];
    result[0] = (unsigned char)innermap.size();
    int pos = 1;

    std::map<std::string, ValueItem>::reverse_iterator iter1;
    for (iter1 = innermap.rbegin(); iter1 != innermap.rend(); ++iter1)
    {
        std::string key = iter1->first;
        int keyLength = key.length();

        memcpy(result + pos, &keyLength, 4);
        pos += 4;
        memcpy(result + pos, key.data(), keyLength);
        pos += keyLength;

        ValueItem item = iter1->second;
        memcpy(result + pos, &item.len, 4);
        pos += 4;
        memcpy(result + pos, item.item, item.len);
        pos += item.len;
    }
    OutputPayload o;
    o.data = result;
    o.length = size;
    return o;
}

ValueItem DotsMqttMessage::GetValue(std::string key)
{
    if (innermap.count(key) == 0)
    {
        Serial.println("not found key");
        throw "not found key";
    }
    return innermap[key];
}

std::string DotsMqttMessage::GetString(std::string key)
{
    ValueItem data = GetValue(key);
    std::string result;
    result.assign((char *)data.item, data.len);
    return result;
}

int DotsMqttMessage::GetInt32(std::string key)
{
    int result;
    ValueItem data = GetValue(key);
    if (data.len != 4)
    {
        throw "type length not equal data length";
    }
    memcpy(&result, data.item, 4);
    return result;
}

long DotsMqttMessage::GetInt64(std::string key)
{
    long result;
    ValueItem data = GetValue(key);
    if (data.len != 8)
    {
        throw "type length not equal data length";
    }
    memcpy(&result, data.item, 8);
    return result;
}

double DotsMqttMessage::GetDouble(std::string key)
{
    double result;
    ValueItem data = GetValue(key);
    if (data.len != 8)
    {
        throw "type length not equal data length";
    }
    memcpy(&result, data.item, 8);
    return result;
}

void DotsMqttMessage::SetString(std::string key, std::string value)
{
    ValueItem item;
    item.len = value.length();
    item.item = new unsigned char[item.len];
    memcpy(item.item, value.c_str(), item.len);
    SetValue(key, item);
}

void DotsMqttMessage::SetInt32(std::string key, int value)
{
    ValueItem item;
    item.len = 4;
    item.item = new unsigned char[item.len];
    memcpy(item.item, &value, item.len);
    SetValue(key, item);
}

void DotsMqttMessage::SetInt64(std::string key, long value)
{
    ValueItem item;
    item.len = 8;
    item.item = new unsigned char[item.len];
    memcpy(item.item, &value, item.len);
    SetValue(key, item);
}

void DotsMqttMessage::SetDouble(std::string key, double value)
{
    ValueItem item;
    item.len = 8;
    item.item = new unsigned char[item.len];
    memcpy(item.item, &value, item.len);
    SetValue(key, item);
}

void DotsMqttMessage::SetBoolean(std::string key, bool value)
{
    ValueItem item;
    item.len = 1;
    item.item = new unsigned char[1]{(value ? (unsigned char)1 : (unsigned char)0)};
    SetValue(key, item);
}

void DotsMqttMessage::SetByteArray(std::string key, unsigned char *value, int length)
{
    ValueItem tempv;
    tempv.len = length;
    tempv.item = new unsigned char[length];
    memcpy(tempv.item, value, length);
    SetValue(key, tempv);
}

void DotsMqttMessage::SetValue(std::string key, ValueItem value)
{
    if (innermap.count(key) == 0)
    {
        innermap.insert(make_pair(key, value));
    }
    else
    {
        innermap[key] = value;
    }
}
