#include "GarminCSC.h"

#include <NimBLEDevice.h>

#define CSC_SERVICE_UUID       "1816"
#define CSC_MEASUREMENT_UUID   "2A5B"
#define CSC_FEATURE_UUID       "2A5C"

static NimBLECharacteristic *cscMeasurement = nullptr;

void GarminCSC::begin() {
  Serial.println("Starting BLE Cycling Speed Sensor");

  // Isolated wheel-only test. This branch starts from the known-good
  // cadence implementation but exposes only wheel revolution data.
  NimBLEDevice::init("ESP32 Speed");

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

  // Wheel Revolution Data Supported only.
  uint16_t features = 0x0001;
  cscFeature->setValue((uint8_t *)&features, sizeof(features));

  cscService->start();

  NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
  advertising->addServiceUUID(CSC_SERVICE_UUID);
  advertising->start();

  Serial.println("Advertising as ESP32 Speed Sensor");
  _lastWheelEventMs = millis();
}

void GarminCSC::update(float speedKph, float cadenceRpm) {
  (void)cadenceRpm;

  const uint32_t now = millis();

  if (speedKph <= 0.0f || cscMeasurement == nullptr) {
    _lastWheelEventMs = now;
    return;
  }

  // Approximate 700C wheel circumference: 2.105 m.
  // This is only used to generate correctly timed wheel revolution events;
  // Garmin derives speed from the revolution count and event timestamp.
  const float wheelRpm = speedKph * 1000.0f / 2.105f / 60.0f;
  if (wheelRpm <= 0.0f) {
    _lastWheelEventMs = now;
    return;
  }

  uint32_t intervalMs = static_cast<uint32_t>(60000.0f / wheelRpm + 0.5f);
  if (intervalMs == 0) {
    intervalMs = 1;
  }

  const uint32_t elapsedMs = now - _lastWheelEventMs;
  if (elapsedMs < intervalMs) {
    return;
  }

  _lastWheelEventMs = now;
  _wheelRevolutions++;
  _wheelEventTime += static_cast<uint16_t>(
      (elapsedMs * 1024UL) / 1000UL);

  // Wheel-only CSC Measurement packet:
  // flags (1) + cumulative wheel revolutions (4) + last wheel event time (2).
  uint8_t packet[7];
  packet[0] = 0x01;
  packet[1] = _wheelRevolutions & 0xFF;
  packet[2] = (_wheelRevolutions >> 8) & 0xFF;
  packet[3] = (_wheelRevolutions >> 16) & 0xFF;
  packet[4] = (_wheelRevolutions >> 24) & 0xFF;
  packet[5] = _wheelEventTime & 0xFF;
  packet[6] = (_wheelEventTime >> 8) & 0xFF;

  cscMeasurement->setValue(packet, sizeof(packet));
  cscMeasurement->notify();

  Serial.printf(
      "Wheel: %lu revs, event time: %u, speed: %.1f km/h\n",
      static_cast<unsigned long>(_wheelRevolutions),
      _wheelEventTime,
      speedKph
  );
}
