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
  const uint32_t now = millis();
  if (cscMeasurement == nullptr) {
    _lastUpdateMs = now;
    return;
  }

  const uint32_t elapsedMs = now - _lastUpdateMs;
  if (elapsedMs == 0) {
    return;
  }
  _lastUpdateMs = now;

  // Convert the live Hyena speed into physical wheel revolutions.
  // The Garmin is configured for the same 2096 mm wheel circumference.
  if (speedKph > 0.0f) {
    const float wheelRevolutions =
        (speedKph / 3.6f) * (elapsedMs / 1000.0f) /
        WHEEL_CIRCUMFERENCE_M;
    _wheelRevolutionAccumulator += wheelRevolutions;
  }

  // Convert live Hyena cadence into physical crank revolutions.
  if (cadenceRpm > 0.0f) {
    _crankRevolutionAccumulator +=
        (cadenceRpm / 60.0f) * (elapsedMs / 1000.0f);
  }

  const uint16_t eventTime =
      static_cast<uint16_t>((static_cast<uint64_t>(now) * 1024ULL) / 1000ULL);

  bool measurementChanged = false;

  if (_wheelRevolutionAccumulator >= 1.0f) {
    const uint32_t wholeRevolutions =
        static_cast<uint32_t>(_wheelRevolutionAccumulator);
    _wheelRevolutionAccumulator -= wholeRevolutions;
    _wheelRevolutions += wholeRevolutions;
    _wheelEventTime = eventTime;
    _haveWheelEvent = true;
    measurementChanged = true;
  }

  if (_crankRevolutionAccumulator >= 1.0f) {
    const uint32_t wholeRevolutions =
        static_cast<uint32_t>(_crankRevolutionAccumulator);
    _crankRevolutionAccumulator -= wholeRevolutions;
    _lastCrankRevolutions =
        static_cast<uint16_t>(_lastCrankRevolutions + wholeRevolutions);
    _crankEventTime = eventTime;
    _haveCrankEvent = true;
    measurementChanged = true;
  }

  // Do not send a zero-speed/zero-cadence event before a real revolution has
  // occurred. Once an event exists, continue notifying so Garmin sees the
  // current cumulative state and can detect the bike stopping.
  if (!measurementChanged && !_haveWheelEvent && !_haveCrankEvent) {
    return;
  }

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
}
