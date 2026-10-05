#include "dust_sensor.h"
#include "config.h"

static constexpr float kDividerRatio = (DIVIDER_R_TOP + DIVIDER_R_BOTTOM) / DIVIDER_R_BOTTOM;

void DustSensor::begin() {
  pinMode(DUST_LED_PIN, OUTPUT);
  digitalWrite(DUST_LED_PIN, HIGH);  // LED off
  analogReadResolution(12);
  zeroVoltage_ = DUST_ZERO_VOLTAGE_DEFAULT;
  lastPulseUs_ = micros();
}

float DustSensor::sampleOnce() {
  digitalWrite(DUST_LED_PIN, LOW);  // LED on
  delayMicroseconds(DUST_SAMPLE_DELAY_US);
  uint32_t mv = analogReadMilliVolts(DUST_VO_PIN);  // eFuse-calibrated
  delayMicroseconds(DUST_PULSE_WIDTH_US - DUST_SAMPLE_DELAY_US);
  digitalWrite(DUST_LED_PIN, HIGH);  // LED off
  return mv / 1000.0f * kDividerRatio;
}

void DustSensor::poll() {
  uint32_t now = micros();
  if (now - lastPulseUs_ < DUST_PULSE_PERIOD_US) return;
  lastPulseUs_ = now;
  sumVoltage_ += sampleOnce();
  count_++;
}

DustSensor::Window DustSensor::takeWindow() {
  Window w{count_, 0, 0};
  if (count_ > 0) {
    w.voltage = sumVoltage_ / count_;
    if (DUST_AUTO_BASELINE && w.voltage < zeroVoltage_) zeroVoltage_ = w.voltage;
    w.density = max(0.0f, (w.voltage - zeroVoltage_) / DUST_SENSITIVITY_V_PER_UG);
  }
  sumVoltage_ = 0;
  count_ = 0;
  return w;
}
