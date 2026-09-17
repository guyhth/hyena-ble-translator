#include "GarminCSC.h"

#include <NimBLEDevice.h>

#define CSC_SERVICE_UUID       "1816"
#define CSC_MEASUREMENT_UUID   "2A5B"
#define CSC_FEATURE_UUID       "2A5C"

static NimBLECharacteristic *cscMeasurement = nullptr;

void GarminCSC::begin() {
  Serial.println("Starting BLE Cycling Speed & Cadence Sensor");

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

  // Bit 0 = Wheel Revolution Data Supported
  // Bit 1 = Crank Revolution Data Supported
  uint16_t features = 0x0003;
  cscFeature->setValue((uint8_t *)&features, sizeof(features));

  cscService->start();

  NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
  advertising->addServiceUUID(CSC_SERVICE_UUID);
  advertising->start();

  Serial.println("Advertising as ESP32 Cycling Speed & Cadence Sensor");
  _lastWheelEventMs = millis();
  _lastCrankEventMs = millis();
}

void GarminCSC::update(float speedKph, float cadenceRpm) {
  const uint32_t now = millis();
  bool eventOccurred = false;

  // Generate wheel revolution events for the hard-coded test speed.
  // 700C wheel circumference is approximated as 2.105 m.
  if (speedKph > 0.0f) {
    const float wheelRpm = speedKph * 1000.0f / 2.105f / 60.0f;
    const uint32_t intervalMs = static_cast<uint32_t>(
        60000.0f / wheelRpm + 0.5f);

    const uint32_t elapsedMs = now - _lastWheelEventMs;
    if (intervalMs > 0 && elapsedMs >= intervalMs) {
      _lastWheelEventMs = now;
      _wheelRevolutions++;
      _wheelEventTime += static_cast<uint16_t>(
          (elapsedMs * 1024UL) / 1000UL);
      eventOccurred = true;
    }
  } else {
    _lastWheelEventMs = now;
  }

  // Preserve the timing model of the known-good cadence implementation.
  if (cadenceRpm <= 0.0f) {
    _lastCrankEventMs = now;
  } else {
    uint32_t intervalMs = static_cast<uint32_t>(60000.0f / cadenceRpm + 0.5f);
    if (intervalMs == 0) {
      intervalMs = 1;
    }

    const uint32_t elapsedMs = now - _lastCrankEventMs;
    if (elapsedMs >= intervalMs) {
      _lastCrankEventMs = now;
      _lastCrankRevolutions++;
      _crankEventTime += static_cast<uint16_t>(
          (elapsedMs * 1024UL) / 1000UL);
      eventOccurred = true;
    }
  }

  if (!eventOccurred || cscMeasurement == nullptr) {
    return;
  }

  // Always send a single combined CSC measurement. Garmin appears to treat
  // the latest notification as the current state; separate wheel-only and
  // crank-only notifications caused the displayed cadence to disappear.
  uint8_t packet[11];
  packet[0] = 0x03;  // Wheel + crank revolution data present

  // Cumulative wheel revolutions (UINT32, little endian)
  packet[1] = _wheelRevolutions & 0xFF;
  packet[2] = (_wheelRevolutions >> 8) & 0xFF;
  packet[3] = (_wheelRevolutions >> 16) & 0xFF;
  packet[4] = (_wheelRevolutions >> 24) & 0xFF;

  // Last wheel event time (UINT16, 1/1024 s, little endian)
  packet[5] = _wheelEventTime & 0xFF;
  packet[6] = (_wheelEventTime >> 8) & 0xFF;

  // Cumulative crank revolutions (UINT16, little endian)
  packet[7] = _lastCrankRevolutions & 0xFF;
  packet[8] = (_lastCrankRevolutions >> 8) & 0xFF;

  // Last crank event time (UINT16, 1/1024 s, little endian)
  packet[9] = _crankEventTime & 0xFF;
  packet[10] = (_crankEventTime >> 8) & 0xFF;

  cscMeasurement->setValue(packet, sizeof(packet));
  cscMeasurement->notify();

  Serial.printf(
      "CSC event: speed %.1f km/h, cadence %.1f RPM, wheel %lu, crank %u\n",
      speedKph,
      cadenceRpm,
      static_cast<unsigned long>(_wheelRevolutions),
      _lastCrankRevolutions
  );
}
