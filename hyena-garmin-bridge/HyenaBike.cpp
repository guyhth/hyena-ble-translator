#include "HyenaBike.h"

#include <NimBLEDevice.h>

namespace {

constexpr char HYENA_SERVICE_UUID[] =
    "48592800-6879-656E-6174-656B2E485550";
constexpr char HYENA_DEVICE_PREFIX[] = "DITK";

String dataToHex(const std::string &data) {
  String result;
  result.reserve(data.length() * 3);

  for (size_t i = 0; i < data.length(); ++i) {
    if (i > 0) {
      result += ' ';
    }
    char byteText[3];
    snprintf(byteText, sizeof(byteText), "%02X",
             static_cast<uint8_t>(data[i]));
    result += byteText;
  }

  return result;
}

void printDevice(const NimBLEAdvertisedDevice *device) {
  const std::string name = device->getName();
  const bool looksLikeHyena =
      name.rfind(HYENA_DEVICE_PREFIX, 0) == 0 ||
      (device->haveServiceUUID() &&
       device->getServiceUUID().toString() == HYENA_SERVICE_UUID);

  Serial.println("----------------------------------------");
  Serial.printf("Name: %s%s\n", name.c_str(),
                looksLikeHyena ? "  <-- Hyena candidate" : "");
  Serial.printf("Address: %s\n", device->getAddress().toString().c_str());
  Serial.printf("RSSI: %d dBm\n", device->getRSSI());

  if (device->haveServiceUUID()) {
    Serial.printf("Service UUID: %s\n",
                  device->getServiceUUID().toString().c_str());
  }

  if (device->haveManufacturerData()) {
    Serial.printf("Manufacturer data: %s\n",
                  dataToHex(device->getManufacturerData()).c_str());
  }

  if (device->haveServiceData()) {
    Serial.printf("Service data: %s\n",
                  dataToHex(device->getServiceData()).c_str());
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
  Serial.printf("Looking for DITK devices / service %s\n", HYENA_SERVICE_UUID);

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
