#include "GarminCSC.h"

#include <NimBLEDevice.h>

#define CSC_SERVICE_UUID       "1816"
#define CSC_MEASUREMENT_UUID   "2A5B"
#define CSC_FEATURE_UUID       "2A5C"

static NimBLECharacteristic *cscMeasurement = nullptr;

void GarminCSC::begin() {
  Serial.println("Starting BLE Cycling Speed & Cadence Sensor");

  // The NimBLE device is shared by the eventual Hyena client and Garmin
  // peripheral. For the initial CSC POC, initialise the device here.
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
  bool wheelEvent = false;
  bool crankEvent = false;

  // Generate wheel revolutions for the hard-coded test speed. A 700C wheel
  // is approximated as 2.105 m circumference.
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
      wheelEvent = true;
    }
  } else {
    _lastWheelEventMs = now;
  }

  // Keep the previously proven cadence event generation unchanged.
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
      _crankEventTime += static_cast<uint16_t>((elapsedMs * 1024UL) / 1000UL);
      crankEvent = true;
    }
  }

  if (cscMeasurement == nullptr) {
    return;
  }

  // Preserve the exact 5-byte cadence-only packet that was previously
  // accepted by Garmin. This isolates the new wheel implementation from the
  // known-good cadence path.
  if (crankEvent) {
    uint8_t packet[5];
    packet[0] = 0x02;
    packet[1] = _lastCrankRevolutions & 0xFF;
    packet[2] = (_lastCrankRevolutions >> 8) & 0xFF;
    packet[3] = _crankEventTime & 0xFF;
    packet[4] = (_crankEventTime >> 8) & 0xFF;

    cscMeasurement->setValue(packet, sizeof(packet));
    cscMeasurement->notify();
  }

  // Send wheel-only measurements separately using the standard 7-byte CSC
  // format. This avoids changing the proven cadence packet while testing
  // Garmin's interpretation of wheel revolution data.
  if (wheelEvent) {
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
  }

  if (wheelEvent || crankEvent) {
    Serial.printf(
        "CSC event: speed %.1f km/h, cadence %.1f RPM, wheel %lu, crank %u\n",
        speedKph,
        cadenceRpm,
        static_cast<unsigned long>(_wheelRevolutions),
        _lastCrankRevolutions
    );
  }
}
