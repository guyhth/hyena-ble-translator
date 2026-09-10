/*
 * Hyena BLE Translator
 *
 * ESP32-C3 firmware for translating Hyena e-bike telemetry into
 * standard Bluetooth cycling services for Garmin devices.
 *
 * Initial milestone: project skeleton. The Garmin CSC layer is
 * deliberately kept separate from the proprietary Hyena protocol
 * decoder so the latter can be developed from captured telemetry.
 */

#include "HyenaBike.h"
#include "GarminCSC.h"
#include "GarminCPS.h"

// Temporary test value for validating cadence timing with Garmin.
// Replace this with telemetry.cadenceRpm once the Hyena BLE client is
// implemented.
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

  // Temporary simulated cadence: Garmin should report approximately
  // 90 RPM. This will be replaced by telemetry.cadenceRpm later.
  csc.update(telemetry.speedKph, TEST_CADENCE_RPM);

  if (telemetry.powerValid) {
    cps.update(telemetry.powerWatts);
  }

  delay(10);
}
