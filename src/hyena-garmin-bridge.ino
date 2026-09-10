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

  csc.update(telemetry.speedKph, telemetry.cadenceRpm);

  if (telemetry.powerValid) {
    cps.update(telemetry.powerWatts);
  }

  delay(10);
}
