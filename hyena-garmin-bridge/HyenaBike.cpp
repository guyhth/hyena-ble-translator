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

HyenaBike *hyenaBikeInstance = nullptr;

void telemetryNotification(NimBLERemoteCharacteristic *characteristic,
                           uint8_t *data, size_t length, bool isNotify) {
  (void)characteristic;
  (void)isNotify;

  if (hyenaBikeInstance != nullptr) {
    hyenaBikeInstance->handleNotification(data, length);
  }
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
  hyenaBikeInstance = this;

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

void HyenaBike::handleNotification(const uint8_t *data, size_t length) {
  if (length < 5 || data[0] != 0x00 || data[1] != 0x00) {
    return;
  }

  const uint16_t packetId =
      (static_cast<uint16_t>(data[2]) << 8) | data[3];
  const uint8_t payloadLength = data[4];

  if (length < 5 + payloadLength) {
    return;
  }

  const uint8_t *payload = data + 5;

  if (packetId == 0x0201 && payloadLength >= 2) {
    const uint16_t rawSpeed =
        static_cast<uint16_t>(payload[0]) |
        (static_cast<uint16_t>(payload[1]) << 8);

    // 0x0201 bytes 0-1 are bike speed in 0.01 km/h.
    _telemetry.speedKph = rawSpeed / 100.0f;
    _telemetry.speedValid = true;
    return;
  }

  if (packetId == 0x0203 && payloadLength >= 2) {
    const uint16_t rawCadence =
        static_cast<uint16_t>(payload[0]) |
        (static_cast<uint16_t>(payload[1]) << 8);

    // 0x0203 bytes 0-1 are cadence, with raw / 40 = RPM.
    _telemetry.cadenceRpm = rawCadence / 40.0f;
    _telemetry.cadenceValid = true;
  }
}
