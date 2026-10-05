#include "dust_sensor.h"
#include "config.h"

static constexpr float kDividerRatio = (DIVIDER_R_TOP + DIVIDER_R_BOTTOM) / DIVIDER_R_BOTTOM;

void DustSensor::begin() {
  pinMode(DUST_LED_PIN, OUTPUT);
  digitalWrite(DUST_LED_PIN, HIGH);  // LED off
  analogReadResolution(12);
  analogSetPinAttenuation(DUST_VO_PIN, ADC_11db);  // full ~0-3.1 V range
  zeroVoltage_ = DUST_ZERO_VOLTAGE_DEFAULT;
  // Core 1 (with loop()), above loop() and the display refresh task.
  xTaskCreatePinnedToCore(taskEntry, "dust", 3072, this, 3, nullptr, 1);
}

void DustSensor::taskEntry(void* self) {
  DustSensor* s = static_cast<DustSensor*>(self);
  TickType_t lastWake = xTaskGetTickCount();
  for (;;) {
    float v = s->sampleOnce();
    portENTER_CRITICAL(&s->mux_);
    s->sumVoltage_ += v;
    s->count_++;
    portEXIT_CRITICAL(&s->mux_);
    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(DUST_PULSE_PERIOD_US / 1000));
  }
}

float DustSensor::sampleOnce() {
  digitalWrite(DUST_LED_PIN, LOW);  // LED on
  delayMicroseconds(DUST_SAMPLE_DELAY_US);
  uint32_t mv = analogReadMilliVolts(DUST_VO_PIN);  // eFuse-calibrated
  delayMicroseconds(DUST_PULSE_WIDTH_US - DUST_SAMPLE_DELAY_US);
  digitalWrite(DUST_LED_PIN, HIGH);  // LED off
  return mv / 1000.0f * kDividerRatio;
}

DustSensor::Window DustSensor::takeWindow() {
  portENTER_CRITICAL(&mux_);
  double sum = sumVoltage_;
  uint32_t count = count_;
  sumVoltage_ = 0;
  count_ = 0;
  portEXIT_CRITICAL(&mux_);

  Window w{count, 0, 0};
  if (count > 0) {
    w.voltage = sum / count;
    if (DUST_AUTO_BASELINE && w.voltage < zeroVoltage_) zeroVoltage_ = w.voltage;
    w.density = max(0.0f, (w.voltage - zeroVoltage_) / DUST_SENSITIVITY_V_PER_UG);
  }
  return w;
}
