#pragma once

#include <Arduino.h>

struct BikeTelemetry {
  float speedKph = 0.0f;
  bool speedValid = false;
  float cadenceRpm = 0.0f;
  bool cadenceValid = false;
  float powerWatts = 0.0f;
  bool powerValid = false;
  uint8_t batterySoc = 0;
};

class HyenaBike {
public:
  void begin();
  void loop();
  const BikeTelemetry &telemetry() const { return _telemetry; }

private:
  BikeTelemetry _telemetry;
};
