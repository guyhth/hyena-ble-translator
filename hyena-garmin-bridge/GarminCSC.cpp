#include "GarminCSC.h"

#include <NimBLEDevice.h>

#define CSC_SERVICE_UUID       "1816"
#define CSC_MEASUREMENT_UUID   "2A5B"
#define CSC_FEATURE_UUID       "2A5C"

static NimBLECharacteristic *cadenceMeasurement = nullptr;
static NimBLECharacteristic *speedMeasurement = nullptr;

void GarminCSC::begin() {
  Serial.println("Starting BLE Cycling Speed & Cadence test sensors");

  NimBLEDevice::init("ESP32 CSC Test");

  NimBLEServer *server = NimBLEDevice::createServer();

  // Diagnostic experiment: expose two separate instances of the standard
  // Cycling Speed & Cadence service. The first is cadence-only and retains
  // the exact 5-byte notification format that previously worked on Garmin.
  NimBLEService *cadenceService = server->createService(CSC_SERVICE_UUID);
  cadenceMeasurement = cadenceService->createCharacteristic(
      CSC_MEASUREMENT_UUID,
      NIMBLE_PROPERTY::NOTIFY
  );
  NimBLECharacteristic *cadenceFeature = cadenceService->createCharacteristic(
      CSC_FEATURE_UUID,
      NIMBLE_PROPERTY::READ
  );
  uint16_t cadenceFeatures = 0x0002;  // Crank Revolution Data Supported
  cadenceFeature->setValue((uint8_t *)&cadenceFeatures, sizeof(cadenceFeatures));
  cadenceService->start();

  // The second service instance is wheel-only. This isolates Garmin's wheel
  // parsing from the cadence implementation.
  NimBLEService *speedService = server->createService(CSC_SERVICE_UUID);
  speedMeasurement = speedService->createCharacteristic(
      CSC_MEASUREMENT_UUID,
      NIMBLE_PROPERTY::NOTIFY
  );
  NimBLECharacteristic *speedFeature = speedService->createCharacteristic(
      CSC_FEATURE_UUID,
      NIMBLE_PROPERTY::READ
  );
  uint16_t speedFeatures = 0x0001;  // Wheel Revolution Data Supported
  speedFeature->setValue((uint8_t *)&speedFeatures, sizeof(speedFeatures));
  speedService->start();

  NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
  advertising->addServiceUUID(CSC_SERVICE_UUID);
  advertising->start();

  Serial.println("Advertising dual CSC test sensors: cadence-only + wheel-only");
  _lastWheelEventMs = millis();
  _lastCrankEventMs = millis();
}

void GarminCSC::update(float speedKph, float cadenceRpm) {
  const uint32_t now = millis();

  // Wheel-only test stream.
  if (speedKph > 0.0f && speedMeasurement != nullptr) {
    // 700C wheel circumference is approximated as 2.105 m.
    const float wheelRpm = speedKph * 1000.0f / 2.105f / 60.0f;
    const uint32_t intervalMs = static_cast<uint32_t>(
        60000.0f / wheelRpm + 0.5f);

    const uint32_t elapsedMs = now - _lastWheelEventMs;
    if (intervalMs > 0 && elapsedMs >= intervalMs) {
      _lastWheelEventMs = now;
      _wheelRevolutions++;
      _wheelEventTime += static_cast<uint16_t>(
          (elapsedMs * 1024UL) / 1000UL);

      uint8_t packet[7];
      packet[0] = 0x01;  // Wheel Revolution Data Present
      packet[1] = _wheelRevolutions & 0xFF;
      packet[2] = (_wheelRevolutions >> 8) & 0xFF;
      packet[3] = (_wheelRevolutions >> 16) & 0xFF;
      packet[4] = (_wheelRevolutions >> 24) & 0xFF;
      packet[5] = _wheelEventTime & 0xFF;
      packet[6] = (_wheelEventTime >> 8) & 0xFF;

      speedMeasurement->setValue(packet, sizeof(packet));
      speedMeasurement->notify();
    }
  } else {
    _lastWheelEventMs = now;
  }

  // Cadence stream: preserve the known-good cadence-only implementation.
  if (cadenceRpm <= 0.0f) {
    _lastCrankEventMs = now;
    return;
  }

  uint32_t intervalMs = static_cast<uint32_t>(60000.0f / cadenceRpm + 0.5f);
  if (intervalMs == 0) {
    intervalMs = 1;
  }

  const uint32_t elapsedMs = now - _lastCrankEventMs;
  if (elapsedMs < intervalMs || cadenceMeasurement == nullptr) {
    return;
  }

  _lastCrankEventMs = now;
  _lastCrankRevolutions++;
  _crankEventTime += static_cast<uint16_t>(
      (elapsedMs * 1024UL) / 1000UL);

  uint8_t packet[5];
  packet[0] = 0x02;  // Crank Revolution Data Present
  packet[1] = _lastCrankRevolutions & 0xFF;
  packet[2] = (_lastCrankRevolutions >> 8) & 0xFF;
  packet[3] = _crankEventTime & 0xFF;
  packet[4] = (_crankEventTime >> 8) & 0xFF;

  cadenceMeasurement->setValue(packet, sizeof(packet));
  cadenceMeasurement->notify();

  Serial.printf(
      "CSC cadence: %.1f RPM, crank %u, event time %u\n",
      cadenceRpm,
      _lastCrankRevolutions,
      _crankEventTime
  );
}
