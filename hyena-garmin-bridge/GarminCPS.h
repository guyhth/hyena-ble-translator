#pragma once

#include <Arduino.h>

class GarminCPS {
public:
  void begin();
  void update(float powerWatts);
};
