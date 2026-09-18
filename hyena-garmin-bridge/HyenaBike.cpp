#include "HyenaBike.h"

#include <NimBLEDevice.h>

namespace {

constexpr char HYENA_SERVICE_UUID[] =
    "48592800-6879-656E-6174-656B2E485550";
constexpr char HYENA_DEVICE_PREFIX[] = "DITK";

NimBLEClient *hyenaClient = nullptr;

class ClientCallbacks : public NimBLEClientCallbacks {
public:
  void onConnect(NimBLEClient *client) override {
    Serial.printf("Hyena GATT connected: %s\n",
                  client->getPeerAddress().toString().c_str());
  }

  void onDisconnect(NimBLEClient *client, int reason) override {
    Serial.printf("Hyena GATT disconnected, reason: %d\n", reason);
  }
};

class ScanCallbacks : public NimBLEScanCallbacks {
public:
  void onResult(const NimBLEAdvertisedDevice *device) override {
    const std::string name = device->getName();

    if (name.rfind(HYENA_DEVICE_PREFIX, 0) != 0) {
      return;
    }

    Serial.println("----------------------------------------");
    Serial.printf("Hyena device found: %s\n", name.c_str());
    Serial.printf("Address: %s\n", device->getAddress().toString().c_str());
    Serial.printf("RSSI: %d dBm\n", device->getRSSI());

    NimBLEDevice::getScan()->stop();

    if (hyenaClient != nullptr) {
      NimBLEDevice::deleteClient(hyenaClient);
    }

    hyenaClient = NimBLEDevice::createClient();
    hyenaClient->setClientCallbacks(new ClientCallbacks(), false);
    hyenaClient->setConnectionParams(12, 12, 0, 150);
    hyenaClient->setConnectTimeout(5 * 1000);

    Serial.println("Connecting to Hyena...");

    if (!hyenaClient->connect(device)) {
      Serial.printf("Hyena GATT connection failed: %s\n",
                    NimBLEUtils::returnCodeToString(
                        hyenaClient->getLastError()).c_str());
      NimBLEDevice::deleteClient(hyenaClient);
      hyenaClient = nullptr;
      return;
    }

    auto *services = hyenaClient->getServices(true);
    Serial.printf("Services discovered: %d\n",
                  static_cast<int>(services.size()));

    for (auto *service : services) {
      Serial.printf("Service: %s\n",
                    service->getUUID().toString().c_str());

      for (auto *characteristic : service->getCharacteristics(true)) {
        Serial.printf("  Characteristic: %s",
                      characteristic->getUUID().toString().c_str());

        std::string properties;
        if (characteristic->canRead()) properties += " READ";
        if (characteristic->canWrite()) properties += " WRITE";
        if (characteristic->canWriteNoResponse()) properties += " WRITE_NR";
        if (characteristic->canNotify()) properties += " NOTIFY";
        if (characteristic->canIndicate()) properties += " INDICATE";

        Serial.printf(" [%s]\n", properties.c_str());
      }
    }

    Serial.println("GATT discovery complete");
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
  // Connection and GATT discovery are performed once from the scan callback.
}
