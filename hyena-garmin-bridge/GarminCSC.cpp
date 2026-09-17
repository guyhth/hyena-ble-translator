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

  // CSC wheel data uses cumulative wheel revolutions and the event time of
  // the most recent wheel revolution. A 700C wheel is approximately 2.105 m
  // in circumference, which corresponds to ~0.130 km/h per wheel RPM.
  if (speedKph > 0.0f) {
    const float wheelRpm = speedKph / 0.130f;
    const float intervalMsFloat = 60000.0f / wheelRpm;
    const uint32_t intervalMs = static_cast<uint32_t>(intervalMsFloat + 0.5f);

    if (intervalMs > 0) {
      const uint32_t elapsedMs = now - _lastWheelEventMs;
      if (elapsedMs >= intervalMs) {
        _lastWheelEventMs = now;
        _wheelRevolutions++;
        _wheelEventTime += static_cast<uint16_t>(
            (elapsedMs * 1024UL) / 1000UL);
      }
    }
  } else {
    _lastWheelEventMs = now;
  }

  // No cadence means no crank events. Reset the timing reference so that
  // a stopped bike does not generate a burst of stale events when cadence
  // resumes.
  if (cadenceRpm <= 0.0f) {
    _lastCrankEventMs = now;
  } else {
    // CSC cadence is represented by the interval between crank revolution
    // events. For example, 60 RPM = 1000 ms/rev and 90 RPM = 667 ms/rev.
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
    }
  }

  // Notify on every update so the Garmin receives the latest cumulative
  // counters. It can calculate speed/cadence from successive events.
  uint8_t packet[11];
  uint8_t flags = 0;

  if (speedKph > 0.0f) {
    flags |= 0x01;  // Wheel revolution data present
  }
  if (cadenceRpm > 0.0f) {
    flags |= 0x02;  // Crank revolution data present
  }

  packet[0] = flags;

  // Cumulative wheel revolutions (UINT32, little endian)
  packet[1] = _wheelRevolutions & 0xFF;
  packet[2] = (_wheelRevolutions >> 8) & 0xFF;
  packet[3] = (_wheelRevolutions >> 16) & 0xFF;
  packet[4] = (_wheelRevolutions >> 24) & 0xFF;

  // Last wheel event time (UINT16, 1/1024 s, little endian)
  const uint16_t wheelEventTime = static_cast<uint16_t>(_wheelEventTime);
  packet[5] = wheelEventTime & 0xFF;
  packet[6] = (wheelEventTime >> 8) & 0xFF;

  // Cumulative crank revolutions (UINT16, little endian)
  packet[7] = _lastCrankRevolutions & 0xFF;
  packet[8] = (_lastCrankRevolutions >> 8) & 0xFF;

  // Last crank event time (UINT16, 1/1024 s, little endian)
  packet[9] = _crankEventTime & 0xFF;
  packet[10] = (_crankEventTime >> 8) & 0xFF;

  if (cscMeasurement != nullptr) {
    cscMeasurement->setValue(packet, sizeof(packet));
    cscMeasurement->notify();
  }

  static uint32_t lastLogMs = 0;
  if (now - lastLogMs >= 1000) {
    lastLogMs = now;
    Serial.printf(
        "CSC: speed %.1f km/h, cadence %.1f RPM, wheel %lu, crank %u\n",
        speedKph,
        cadenceRpm,
        static_cast<unsigned long>(_wheelRevolutions),
        _lastCrankRevolutions
    );
  }
}
