#include "GarminCSC.h"

#include <NimBLEDevice.h>

#define CSC_SERVICE_UUID       "1816"
#define CSC_MEASUREMENT_UUID   "2A5B"
#define CSC_FEATURE_UUID       "2A5C"

static NimBLECharacteristic *cscMeasurement = nullptr;

void GarminCSC::begin() {
  Serial.println("Starting BLE Cycling Cadence Sensor");

  // The NimBLE device is shared by the eventual Hyena client and Garmin
  // peripheral. For the initial cadence POC, initialise the device here.
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

  // Bit 1 = Crank Revolution Data Supported
  uint16_t features = 0x0002;
  cscFeature->setValue((uint8_t *)&features, sizeof(features));

  cscService->start();

  NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
  advertising->addServiceUUID(CSC_SERVICE_UUID);
  advertising->start();

  Serial.println("Advertising as ESP32 Cadence");
  _lastCrankEventMs = millis();
}

void GarminCSC::update(float speedKph, float cadenceRpm) {
  (void)speedKph;

  // No cadence means no crank events. Reset the timing reference so that
  // a stopped bike does not generate a burst of stale events when cadence
  // resumes.
  if (cadenceRpm <= 0.0f) {
    _lastCrankEventMs = millis();
    return;
  }

  // CSC cadence is represented by the interval between crank revolution
  // events. For example, 60 RPM = 1000 ms/rev and 90 RPM = 667 ms/rev.
  uint32_t intervalMs = static_cast<uint32_t>(60000.0f / cadenceRpm + 0.5f);
  if (intervalMs == 0) {
    intervalMs = 1;
  }

  const uint32_t now = millis();
  const uint32_t elapsedMs = now - _lastCrankEventMs;

  if (elapsedMs < intervalMs) {
    return;
  }

  // Record the actual event time in the CSC 1/1024-second timebase.
  // UINT16 wraps naturally, as required by the CSC specification.
  _lastCrankEventMs = now;
  _lastCrankRevolutions++;
  _crankEventTime += static_cast<uint16_t>((elapsedMs * 1024UL) / 1000UL);

  uint8_t packet[5];
  packet[0] = 0x02;  // Crank revolution data present

  // Cumulative crank revolutions (UINT16, little endian)
  packet[1] = _lastCrankRevolutions & 0xFF;
  packet[2] = (_lastCrankRevolutions >> 8) & 0xFF;

  // Last crank event time (UINT16, 1/1024 s, little endian)
  packet[3] = _crankEventTime & 0xFF;
  packet[4] = (_crankEventTime >> 8) & 0xFF;

  if (cscMeasurement != nullptr) {
    cscMeasurement->setValue(packet, sizeof(packet));
    cscMeasurement->notify();
  }

  Serial.printf(
      "Crank: %u revs, event time: %u, cadence: %.1f RPM\n",
      _lastCrankRevolutions,
      _crankEventTime,
      cadenceRpm
  );
}
