#include "GarminCSC.h"

#include <NimBLEDevice.h>

#define CSC_SERVICE_UUID       "1816"
#define CSC_MEASUREMENT_UUID   "2A5B"
#define CSC_FEATURE_UUID       "2A5C"
#define CSC_CONTROL_POINT_UUID "2A55"

static NimBLECharacteristic *cscMeasurement = nullptr;

void GarminCSC::begin() {
  Serial.println("Starting BLE Cycling Speed Sensor");

  // Combined wheel + crank CSC test. Both paths have been independently
  // verified by the Epix Gen 2, and the SC Control Point is present because
  // Garmin requires it to activate the sensor for an activity.
  NimBLEDevice::init("ESP32 Speed Cadence");

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

  // Wheel Revolution Data Supported + Crank Revolution Data Supported.
  uint16_t features = 0x0003;
  cscFeature->setValue((uint8_t *)&features, sizeof(features));

  // Required when wheel revolution data is supported.
  cscService->createCharacteristic(
      CSC_CONTROL_POINT_UUID,
      NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::INDICATE
  );

  cscService->start();

  NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
  advertising->addServiceUUID(CSC_SERVICE_UUID);
  advertising->start();

  Serial.println("Advertising as ESP32 Speed/Cadence Sensor");
  _lastWheelEventMs = millis();
  _lastCrankEventMs = millis();
}

void GarminCSC::update(float speedKph, float cadenceRpm) {
  (void)speedKph;
  (void)cadenceRpm;

  const uint32_t now = millis();
  if (cscMeasurement == nullptr) {
    return;
  }

  // Deterministic wheel test: exactly one revolution per second.
  // At 2096 mm circumference Garmin should display ~7.55 km/h.
  const uint32_t wheelElapsedMs = now - _lastWheelEventMs;
  if (wheelElapsedMs >= 1000) {
    _lastWheelEventMs += 1000;
    _wheelRevolutions++;
    _wheelEventTime += 1024;
  }

  // Deterministic cadence test: 90 RPM, approximately one revolution every
  // 667 ms. This retains the timing model from the known-good cadence test.
  const uint32_t crankElapsedMs = now - _lastCrankEventMs;
  if (crankElapsedMs >= 667) {
    _lastCrankEventMs += 667;
    _lastCrankRevolutions++;
    _crankEventTime += 683; // 667 ms * 1024 / 1000, rounded
  }

  if (wheelElapsedMs >= 1000 || crankElapsedMs >= 667) {
    // Combined CSC Measurement: flags + wheel revolution data + crank data.
    uint8_t packet[11];
    packet[0] = 0x03;

    packet[1] = _wheelRevolutions & 0xFF;
    packet[2] = (_wheelRevolutions >> 8) & 0xFF;
    packet[3] = (_wheelRevolutions >> 16) & 0xFF;
    packet[4] = (_wheelRevolutions >> 24) & 0xFF;
    packet[5] = _wheelEventTime & 0xFF;
    packet[6] = (_wheelEventTime >> 8) & 0xFF;

    packet[7] = _lastCrankRevolutions & 0xFF;
    packet[8] = (_lastCrankRevolutions >> 8) & 0xFF;
    packet[9] = _crankEventTime & 0xFF;
    packet[10] = (_crankEventTime >> 8) & 0xFF;

    cscMeasurement->setValue(packet, sizeof(packet));
    cscMeasurement->notify();

    Serial.printf(
        "CSC: wheel=%lu @ %u, crank=%u @ %u (test: 7.55 km/h / 90 RPM)\n",
        static_cast<unsigned long>(_wheelRevolutions),
        _wheelEventTime,
        _lastCrankRevolutions,
        _crankEventTime
    );
  }
}
