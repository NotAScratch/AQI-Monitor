#pragma once

// AQI from PM2.5 (ug/m3). The scale is chosen by AQI_STANDARD in config.h:
// US EPA (2024 breakpoints) or India CPCB.
int aqiFromPm25(float pm25);
const char* aqiCategory(int aqi);
