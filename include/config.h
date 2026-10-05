#pragma once

#define FIRMWARE_VERSION "0.1.0"

// ---------------------------------------------------------------------------
// Pins (ESP32-S3). The sensor output MUST be on an ADC1 pin (GPIO1-10):
// ADC2 cannot be read while Wi-Fi is active.
// ---------------------------------------------------------------------------
#define DUST_VO_PIN   4   // GP2Y10 pin 5 (Vo), through the voltage divider
#define DUST_LED_PIN  5   // GP2Y10 pin 3 (LED), LOW = IR LED on

// Voltage divider between sensor Vo and the ADC pin.
// Vo -- R_TOP --+-- R_BOTTOM -- GND, ADC reads the midpoint.
// Default 10k / 20k scales the sensor's ~0-3.8 V output to ~0-2.5 V.
#define DIVIDER_R_TOP     10000.0f
#define DIVIDER_R_BOTTOM  20000.0f

// ---------------------------------------------------------------------------
// Sensor timing (from the Sharp datasheet): 0.32 ms pulse every 10 ms,
// sample 0.28 ms after the LED turns on.
// ---------------------------------------------------------------------------
#define DUST_PULSE_PERIOD_US   10000
#define DUST_SAMPLE_DELAY_US   280
#define DUST_PULSE_WIDTH_US    320

// ---------------------------------------------------------------------------
// Calibration
// ---------------------------------------------------------------------------
// Output voltage in clean air (Voc). Datasheet typical is 0.9 V, real parts
// range ~0.6-1.5 V. Measure yours in clean air and set it here, or from the
// ThingsBoard shared attribute "dustZeroVoltage".
#define DUST_ZERO_VOLTAGE_DEFAULT   0.60f
// Sensitivity: datasheet typical 0.5 V per 100 ug/m3.
#define DUST_SENSITIVITY_V_PER_UG   0.005f
// Lower Voc automatically when a cleaner-than-baseline window is seen
// (Sharp application note method).
#define DUST_AUTO_BASELINE          true

// ---------------------------------------------------------------------------
// Reporting
// ---------------------------------------------------------------------------
#define TELEMETRY_INTERVAL_S_DEFAULT  10   // overridable via "telemetryIntervalSec"
#define AQI_AVG_WINDOWS               30   // rolling average used for AQI (30 x 10 s = 5 min)
