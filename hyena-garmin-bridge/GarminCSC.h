#pragma once

#include <Arduino.h>

class GarminCSC {
public:
  void begin();
  void update(float speedKph, float cadenceRpm);

private:
  static constexpr float WHEEL_CIRCUMFERENCE_M = 2.096f;

  uint32_t _wheelRevolutions = 0;
  uint16_t _lastCrankRevolutions = 0;
  uint16_t _wheelEventTime = 0;
  uint16_t _crankEventTime = 0;
  uint32_t _lastUpdateMs = 0;
  float _wheelRevolutionAccumulator = 0.0f;
  float _crankRevolutionAccumulator = 0.0f;
  bool _haveWheelEvent = false;
  bool _haveCrankEvent = false;
};
