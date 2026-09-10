#include "GarminCPS.h"

void GarminCPS::begin() {
  // Standard Bluetooth Cycling Power Service GATT server will be
  // initialised here once the Hyena power telemetry has been identified.
}

void GarminCPS::update(float powerWatts) {
  // Publish instantaneous power in watts using the standard CPS
  // measurement format.
  (void)powerWatts;
}
