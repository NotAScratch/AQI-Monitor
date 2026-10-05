#pragma once
#include <Arduino.h>

// Sharp GP2Y1010AU0F optical dust sensor.
// Call poll() as often as possible from loop(); it fires one LED pulse every
// DUST_PULSE_PERIOD_US and accumulates the readings until takeWindow().
class DustSensor {
public:
  void begin();
  void poll();

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
  float sampleOnce();

  float zeroVoltage_ = 0;
  uint32_t lastPulseUs_ = 0;
  double sumVoltage_ = 0;
  uint32_t count_ = 0;
};
