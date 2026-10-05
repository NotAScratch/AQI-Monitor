#pragma once

// US EPA AQI from PM2.5 (ug/m3), using the 2024 breakpoints.
int aqiFromPm25(float pm25);
const char* aqiCategory(int aqi);
