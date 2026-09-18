#include "HyenaBike.h"

#include <NimBLEDevice.h>

namespace {

void printDevice(const NimBLEAdvertisedDevice *device) {
  Serial.println("----------------------------------------");
  Serial.printf("Name: %s\n", device->getName().c_str());
  Serial.printf("Address: %s\n", device->getAddress().toString().c_str());
  Serial.printf("RSSI: %d dBm\n", device->getRSSI());

  if (device->haveServiceUUID()) {
    Serial.printf("Service UUID: %s\n",
                  device->getServiceUUID().toString().c_str());
  }

  if (device->haveManufacturerData()) {
    Serial.printf("Manufacturer data: %s\n",
                  NimBLEUtils::dataToHexString(
                      device->getManufacturerData()).c_str());
  }

  if (device->haveServiceData()) {
    Serial.printf("Service data: %s\n",
                  NimBLEUtils::dataToHexString(
                      device->getServiceData()).c_str());
  }
}

class ScanCallbacks : public NimBLEScanCallbacks {
public:
  void onResult(const NimBLEAdvertisedDevice *device) override {
    printDevice(device);
  }
};

}  // namespace

void HyenaBike::begin() {
  Serial.println("Starting Hyena BLE scan");

  NimBLEDevice::init("");

  NimBLEScan *scan = NimBLEDevice::getScan();
  scan->setScanCallbacks(new ScanCallbacks(), false);
  scan->setActiveScan(true);
  scan->setInterval(100);
  scan->setWindow(80);

  Serial.println("Scanning for 10 seconds...");
  scan->start(10 * 1000, false);

  Serial.println("Hyena BLE scan complete");
}

void HyenaBike::loop() {
  // No connection or notification handling in this test.
}
