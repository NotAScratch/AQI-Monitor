#include "aqi.h"
#include "config.h"
#include <math.h>

namespace {
struct Breakpoint { float cLo, cHi; int iLo, iHi; const char* label; };

#if AQI_STANDARD == 1
// EPA PM2.5 breakpoints, effective May 2024.
constexpr Breakpoint kPm25[] = {
  {0.0f,   9.0f,   0,   50,  "Good"},
  {9.1f,   35.4f,  51,  100, "Moderate"},
  {35.5f,  55.4f,  101, 150, "Unhealthy for Sensitive Groups"},
  {55.5f,  125.4f, 151, 200, "Unhealthy"},
  {125.5f, 225.4f, 201, 300, "Very Unhealthy"},
  {225.5f, 325.4f, 301, 500, "Hazardous"},
};
#elif AQI_STANDARD == 2
// India CPCB National AQI, PM2.5 sub-index. The top band is open-ended in the
// standard; 500 ug/m3 is used as its upper end.
constexpr Breakpoint kPm25[] = {
  {0.0f,   30.0f,  0,   50,  "Good"},
  {30.1f,  60.0f,  51,  100, "Satisfactory"},
  {60.1f,  90.0f,  101, 200, "Moderate"},
  {90.1f,  120.0f, 201, 300, "Poor"},
  {120.1f, 250.0f, 301, 400, "Very Poor"},
  {250.1f, 500.0f, 401, 500, "Severe"},
};
#else
#error "AQI_STANDARD must be 1 (US EPA) or 2 (India CPCB)"
#endif
}  // namespace

int aqiFromPm25(float pm25) {
  if (pm25 < 0) pm25 = 0;
  float c = floorf(pm25 * 10.0f) / 10.0f;  // truncate to 0.1 ug/m3
  for (const auto& bp : kPm25) {
    if (c <= bp.cHi) {
      if (c < bp.cLo) c = bp.cLo;  // guard float rounding just below a breakpoint
      return lroundf((bp.iHi - bp.iLo) / (bp.cHi - bp.cLo) * (c - bp.cLo) + bp.iLo);
    }
  }
  return 500;  // beyond the index
}

const char* aqiCategory(int aqi) {
  for (const auto& bp : kPm25) {
    if (aqi <= bp.iHi) return bp.label;
  }
  return kPm25[sizeof(kPm25) / sizeof(kPm25[0]) - 1].label;
}
