#include "GarminCSC.h"

#include <NimBLEDevice.h>

#define CSC_SERVICE_UUID       "1816"
#define CSC_MEASUREMENT_UUID   "2A5B"
#define CSC_FEATURE_UUID       "2A5C"

static NimBLECharacteristic *cscMeasurement = nullptr;

void GarminCSC::begin() {
  Serial.println("Starting BLE Cycling Speed & Cadence Sensor");

  // Restore the original known-good cadence-only configuration.
  NimBLEDevice::init("ESP32 Cadence");

  NimBLEServer *server = NimBLEDevice::createServer();
  NimBLEService *cscService = server->createService(CSC_SERVICE_UUID);

  cscMeasurement = cscService->createCharacteristic(
      CSC_MEASUREMENT_UUID,
      NIMBLE_PROPERTY::NOTIFY
  );

  NimBLECharacteristic *cscFeature = cscService->createCharacteristic(
      CSC_FEATURE_UUID,
      NIMBLE_PROPERTY::READ
  );

  // Crank Revolution Data Supported only.
  uint16_t features = 0x0002;
  cscFeature->setValue((uint8_t *)&features, sizeof(features));

  cscService->start();

  NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
  advertising->addServiceUUID(CSC_SERVICE_UUID);
  advertising->start();

  Serial.println("Advertising as ESP32 Cadence Sensor");
  _lastCrankEventMs = millis();
}

void GarminCSC::update(float speedKph, float cadenceRpm) {
  (void)speedKph;

  const uint32_t now = millis();

  if (cadenceRpm <= 0.0f) {
    _lastCrankEventMs = now;
    return;
  }

  uint32_t intervalMs = static_cast<uint32_t>(60000.0f / cadenceRpm + 0.5f);
  if (intervalMs == 0) {
    intervalMs = 1;
  }

  const uint32_t elapsedMs = now - _lastCrankEventMs;
  if (elapsedMs < intervalMs || cscMeasurement == nullptr) {
    return;
  }

  _lastCrankEventMs = now;
  _lastCrankRevolutions++;
  _crankEventTime += static_cast<uint16_t>(
      (elapsedMs * 1024UL) / 1000UL);

  // Exact 5-byte cadence-only packet that was previously accepted by Garmin.
  uint8_t packet[5];
  packet[0] = 0x02;
  packet[1] = _lastCrankRevolutions & 0xFF;
  packet[2] = (_lastCrankRevolutions >> 8) & 0xFF;
  packet[3] = _crankEventTime & 0xFF;
  packet[4] = (_crankEventTime >> 8) & 0xFF;

  cscMeasurement->setValue(packet, sizeof(packet));
  cscMeasurement->notify();

  Serial.printf(
      "Crank: %u revs, event time: %u, cadence: %.1f RPM\n",
      _lastCrankRevolutions,
      _crankEventTime,
      cadenceRpm
  );
}
