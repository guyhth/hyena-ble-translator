#include "GarminCSC.h"

void GarminCSC::begin() {
  // Standard Bluetooth Cycling Speed & Cadence GATT server will be
  // initialised here. The implementation should preserve the known
  // working Garmin cadence proof-of-concept behaviour.
}

void GarminCSC::update(float speedKph, float cadenceRpm) {
  // Convert the internal SI/engineering values into the standard CSC
  // measurement representation expected by Garmin.
  (void)speedKph;
  (void)cadenceRpm;
}
