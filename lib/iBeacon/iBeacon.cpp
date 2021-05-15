#include "iBeacon.h"

BLEAdvertising *pAdvertising;

void StartDeviceIDBeacon(char *deviceIDWithoutCompany)
{
    // Create the BLE Device
    BLEDevice::init("DotsIoT");
    pAdvertising = BLEDevice::getAdvertising();

    BLEDevice::startAdvertising();
    pAdvertising->setAdvertisementType(ADV_TYPE_NONCONN_IND);
    pAdvertising->addServiceUUID(BLEUUID("DYKJ"));
    pAdvertising->addServiceUUID(BLEUUID(deviceIDWithoutCompany));
    pAdvertising->start();
}

void StopBeacon()
{
    pAdvertising->stop();
}