#pragma once
#include <Arduino.h>

// Sharp GP2Y1010AU0F optical dust sensor.
// begin() starts a high-priority task that fires one LED pulse every
// DUST_PULSE_PERIOD_US and accumulates the readings until takeWindow().
// Running it as its own task keeps the 0.28 ms sample point exact even while
// loop() is busy with MQTT or the display.
class DustSensor {
public:
  void begin();

  struct Window {
    uint32_t samples;
    float voltage;   // mean sensor output voltage (before the divider), V
    float density;   // estimated dust density, ug/m3
  };
  // Returns the mean of all pulses since the previous call and starts a new window.
  Window takeWindow();

  void setZeroVoltage(float v) { zeroVoltage_ = v; }
  float zeroVoltage() const { return zeroVoltage_; }

private:
  static void taskEntry(void* self);
  float sampleOnce();

  float zeroVoltage_ = 0;
  portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
  double sumVoltage_ = 0;
  uint32_t count_ = 0;
};
