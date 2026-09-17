#include "GarminCSC.h"

#include <NimBLEDevice.h>

#define CSC_SERVICE_UUID       "1816"
#define CSC_MEASUREMENT_UUID   "2A5B"
#define CSC_FEATURE_UUID       "2A5C"
#define CSC_CONTROL_POINT_UUID "2A55"

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

  // The CSC specification requires the SC Control Point when wheel
  // revolution data is supported. Garmin may discover/connect to a CSC
  // sensor without it, but may not activate it for an activity. For this
  // diagnostic branch we expose the characteristic with the required
  // Write + Indicate properties. No procedures are needed for the test.
  cscService->createCharacteristic(
      CSC_CONTROL_POINT_UUID,
      NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::INDICATE
  );

  cscService->start();

  NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
  advertising->addServiceUUID(CSC_SERVICE_UUID);
  advertising->start();

  Serial.println("Advertising as ESP32 Speed Sensor");
  _lastWheelEventMs = millis();
}

void GarminCSC::update(float speedKph, float cadenceRpm) {
  (void)speedKph;
  (void)cadenceRpm;

  const uint32_t now = millis();

  // Deliberately ignore the live input and generate exactly one wheel
  // revolution per second. With a 2.105 m circumference Garmin should
  // calculate approximately 7.58 km/h from the revolution count/timestamp.
  const uint32_t elapsedMs = now - _lastWheelEventMs;
  if (elapsedMs < 1000 || cscMeasurement == nullptr) {
    return;
  }

  // Keep the test timing deterministic rather than accumulating millis()
  // jitter. Each notification represents exactly one second since the
  // previous wheel event.
  _lastWheelEventMs += 1000;
  _wheelRevolutions++;
  _wheelEventTime += 1024;

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
      "Wheel: %lu revs, event time: %u, test speed: 7.58 km/h\n",
      static_cast<unsigned long>(_wheelRevolutions),
      _wheelEventTime
  );
}
