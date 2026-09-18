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

  // Feed the latest live Hyena telemetry into the Garmin-facing CSC service.
  csc.update(
      telemetry.speedValid ? telemetry.speedKph : 0.0f,
      telemetry.cadenceValid ? telemetry.cadenceRpm : 0.0f
  );

  if (telemetry.powerValid) {
    cps.update(telemetry.powerWatts);
  }

  delay(10);
}
