#include "HyenaBike.h"

#include <NimBLEDevice.h>

namespace {

constexpr char HYENA_SERVICE_UUID[] =
    "48592800-6879-656E-6174-656B2E485550";
constexpr char HYENA_TELEMETRY_UUID[] =
    "4859FF01-6879-656E-6174-656B2E485550";
constexpr char HYENA_DEVICE_PREFIX[] = "DITK";

NimBLEClient *hyenaClient = nullptr;
NimBLEAddress hyenaAddress;
NimBLERemoteCharacteristic *telemetryCharacteristic = nullptr;
bool hyenaFound = false;
bool connectionAttempted = false;

String dataToHex(const uint8_t *data, size_t length) {
  String result;
  result.reserve(length * 3);

  for (size_t i = 0; i < length; ++i) {
    if (i > 0) {
      result += ' ';
    }

    char byteText[3];
    snprintf(byteText, sizeof(byteText), "%02X", data[i]);
    result += byteText;
  }

  return result;
}

void telemetryNotification(NimBLERemoteCharacteristic *characteristic,
                           uint8_t *data, size_t length, bool isNotify) {
  Serial.printf("Hyena notification [%s] %u bytes: %s\n",
                isNotify ? "NOTIFY" : "INDICATE",
                static_cast<unsigned>(length),
                dataToHex(data, length).c_str());
}

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

bool subscribeToTelemetry() {
  NimBLERemoteService *service = hyenaClient->getService(HYENA_SERVICE_UUID);
  if (service == nullptr) {
    Serial.printf("Hyena service not found: %s\n", HYENA_SERVICE_UUID);
    return false;
  }

  telemetryCharacteristic = service->getCharacteristic(HYENA_TELEMETRY_UUID);
  if (telemetryCharacteristic == nullptr) {
    Serial.printf("Hyena telemetry characteristic not found: %s\n",
                  HYENA_TELEMETRY_UUID);
    return false;
  }

  if (!telemetryCharacteristic->canNotify()) {
    Serial.println("Hyena telemetry characteristic does not support NOTIFY");
    return false;
  }

  Serial.printf("Subscribing to Hyena telemetry: %s\n",
                HYENA_TELEMETRY_UUID);

  if (!telemetryCharacteristic->subscribe(true, telemetryNotification)) {
    Serial.println("Hyena telemetry subscription failed");
    return false;
  }

  Serial.println("Hyena telemetry subscription active");
  return true;
}

void discoverServices() {
  const auto services = hyenaClient->getServices(true);

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

class ScanCallbacks : public NimBLEScanCallbacks {
public:
  void onResult(const NimBLEAdvertisedDevice *device) override {
    const std::string name = device->getName();

    if (name.rfind(HYENA_DEVICE_PREFIX, 0) != 0 || hyenaFound) {
      return;
    }

    Serial.println("----------------------------------------");
    Serial.printf("Hyena device found: %s\n", name.c_str());
    Serial.printf("Address: %s\n", device->getAddress().toString().c_str());
    Serial.printf("RSSI: %d dBm\n", device->getRSSI());

    hyenaAddress = device->getAddress();
    hyenaFound = true;

    NimBLEDevice::getScan()->stop();
    Serial.println("Hyena device saved; connection will start from loop()");
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
}

void HyenaBike::loop() {
  if (!hyenaFound || connectionAttempted ||
      NimBLEDevice::getScan()->isScanning()) {
    return;
  }

  connectionAttempted = true;

  if (hyenaClient != nullptr) {
    NimBLEDevice::deleteClient(hyenaClient);
  }

  hyenaClient = NimBLEDevice::createClient();
  hyenaClient->setClientCallbacks(new ClientCallbacks(), false);
  hyenaClient->setConnectionParams(12, 12, 0, 150);
  hyenaClient->setConnectTimeout(5 * 1000);

  Serial.println("Connecting to Hyena...");

  if (!hyenaClient->connect(hyenaAddress)) {
    Serial.printf("Hyena GATT connection failed: %s\n",
                  NimBLEUtils::returnCodeToString(
                      hyenaClient->getLastError()));
    NimBLEDevice::deleteClient(hyenaClient);
    hyenaClient = nullptr;
    return;
  }

  discoverServices();
  subscribeToTelemetry();
}
