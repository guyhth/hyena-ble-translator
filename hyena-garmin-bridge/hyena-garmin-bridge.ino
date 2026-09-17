/*
 * Hyena BLE Translator
 *
 * ESP32-C3 firmware for translating Hyena e-bike telemetry into
 * standard Bluetooth cycling services for Garmin devices.
 *
 * Initial milestone: dummy Garmin sensor data. The Garmin BLE layer is
 * deliberately kept separate from the proprietary Hyena protocol decoder
 * so the two can be developed and tested independently.
 */

#include "HyenaBike.h"
#include "GarminCSC.h"
#include "GarminCPS.h"

// Temporary hard-coded values for validating Garmin sensor profiles.
// Replace these with Hyena telemetry once the BLE client is integrated.
const float TEST_SPEED_KPH = 25.0f;
const float TEST_CADENCE_RPM = 90.0f;

HyenaBike bike;
GarminCSC csc;
GarminCPS cps;

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("Hyena BLE Translator");
  Serial.println("ESP32-C3 starting...");

  csc.begin();
  cps.begin();
  bike.begin();
}

void loop() {
  bike.loop();

  const BikeTelemetry &telemetry = bike.telemetry();

  // Temporary simulated speed and cadence. These will be replaced by
  // telemetry.speedKph and telemetry.cadenceRpm later.
  csc.update(TEST_SPEED_KPH, TEST_CADENCE_RPM);

  if (telemetry.powerValid) {
    cps.update(telemetry.powerWatts);
  }

  delay(10);
}
