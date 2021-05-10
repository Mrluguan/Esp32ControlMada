#include "iBeacon.h"

#define BEACON_UUID "D86EA1A7-71FA-4595-9860-883C3BAC366E"

BLEAdvertising *pAdvertising;

void StartDeviceIDBeacon(char *deviceID)
{
    // Create the BLE Device
    BLEDevice::init(deviceID);
    // Create the BLE Server
    BLEServer *pServer = BLEDevice::createServer(); // <-- no longer required to instantiate BLEServer, less flash and ram usage
    pAdvertising = BLEDevice::getAdvertising();
    BLEDevice::startAdvertising();

    BLEBeacon oBeacon = BLEBeacon();
    oBeacon.setManufacturerId(65535); // fake Apple 0x004C LSB (ENDIAN_CHANGE_U16!)
    oBeacon.setProximityUUID(BLEUUID(BEACON_UUID));
    oBeacon.setMajor(22);
    oBeacon.setMinor(154);
    BLEAdvertisementData oAdvertisementData = BLEAdvertisementData();
    BLEAdvertisementData oScanResponseData = BLEAdvertisementData();

    oAdvertisementData.setFlags(0x04); // BR_EDR_NOT_SUPPORTED 0x04

    std::string strServiceData = "";

    strServiceData += (char)26;   // Len
    strServiceData += (char)0xFF; // Type
    strServiceData += oBeacon.getData();
    oAdvertisementData.addData(strServiceData);

    pAdvertising->setAdvertisementData(oAdvertisementData);
    pAdvertising->setScanResponseData(oScanResponseData);
    pAdvertising->setAdvertisementType(ADV_TYPE_NONCONN_IND);

    // Start advertising
    pAdvertising->start();
}

void StopBeacon()
{
    pAdvertising->stop();
}