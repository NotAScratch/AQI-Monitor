#include "aqi.h"
#include <math.h>

namespace {
struct Breakpoint { float cLo, cHi; int iLo, iHi; };

// EPA PM2.5 breakpoints, effective May 2024.
constexpr Breakpoint kPm25[] = {
  {0.0f,   9.0f,   0,   50},
  {9.1f,   35.4f,  51,  100},
  {35.5f,  55.4f,  101, 150},
  {55.5f,  125.4f, 151, 200},
  {125.5f, 225.4f, 201, 300},
  {225.5f, 325.4f, 301, 500},
};
}  // namespace

int aqiFromPm25(float pm25) {
  if (pm25 < 0) pm25 = 0;
  float c = floorf(pm25 * 10.0f) / 10.0f;  // EPA truncates to 0.1 ug/m3
  for (const auto& bp : kPm25) {
    if (c <= bp.cHi) {
      if (c < bp.cLo) c = bp.cLo;  // guard float rounding just below a breakpoint
      return lroundf((bp.iHi - bp.iLo) / (bp.cHi - bp.cLo) * (c - bp.cLo) + bp.iLo);
    }
  }
  return 500;  // beyond the index
}

const char* aqiCategory(int aqi) {
  if (aqi <= 50)  return "Good";
  if (aqi <= 100) return "Moderate";
  if (aqi <= 150) return "Unhealthy for Sensitive Groups";
  if (aqi <= 200) return "Unhealthy";
  if (aqi <= 300) return "Very Unhealthy";
  return "Hazardous";
}
