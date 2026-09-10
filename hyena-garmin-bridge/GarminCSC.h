#pragma once

#include <Arduino.h>

class GarminCSC {
public:
  void begin();
  void update(float speedKph, float cadenceRpm);

private:
  uint32_t _wheelRevolutions = 0;
  uint16_t _lastCrankRevolutions = 0;
  uint32_t _lastEventTime = 0;
};
