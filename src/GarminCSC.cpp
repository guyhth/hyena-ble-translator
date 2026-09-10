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
  _lastEventTime = millis();
}

void GarminCSC::update(float speedKph, float cadenceRpm) {
  (void)speedKph;

  // The original POC generated one crank revolution per second, which
  // Garmin interpreted as 60 RPM. Preserve that behaviour as the known
  // good baseline until Hyena telemetry is connected.
  if (millis() - _lastEventTime >= 1000) {
    _lastEventTime += 1000;
    _lastCrankRevolutions++;

    // CSC event time uses 1/1024 second units.
    uint16_t eventTime = static_cast<uint16_t>(_lastEventTime * 1024UL / 1000UL);

    uint8_t packet[5];
    packet[0] = 0x02;  // Crank revolution data present

    packet[1] = _lastCrankRevolutions & 0xFF;
    packet[2] = (_lastCrankRevolutions >> 8) & 0xFF;

    packet[3] = eventTime & 0xFF;
    packet[4] = (eventTime >> 8) & 0xFF;

    if (cscMeasurement != nullptr) {
      cscMeasurement->setValue(packet, sizeof(packet));
      cscMeasurement->notify();
    }

    Serial.printf(
        "Crank: %u revs, event time: %u, cadence: %.1f RPM\n",
        _lastCrankRevolutions,
        eventTime,
        cadenceRpm
    );
  }
}
