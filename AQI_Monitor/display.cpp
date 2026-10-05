#include "display.h"
#include "config.h"

#include <SPI.h>
#include <DMD32Plus.h>
#include "fonts/Arial_black_16.h"

static DMD dmd(1, 1, DMD_PIN_OE, DMD_PIN_A, DMD_PIN_B, DMD_PIN_CLK, DMD_PIN_LAT, DMD_PIN_R_DATA);

static char nextText[64] = "";
static bool scrolling = false;
static uint32_t lastStepMs = 0;

// The panel shows one row group at a time and must be refreshed continuously.
// One row group per tick (1 ms) = 250 Hz full refresh. Runs on core 1 so it
// never competes with Wi-Fi on core 0, and sleeps between refreshes so the
// idle task (and its watchdog) still runs.
static void scanTask(void*) {
  for (;;) {
    dmd.scanDisplayBySPI();
    vTaskDelay(1);
  }
}

void Display::begin() {
  pinMode(SS, INPUT_PULLUP);  // see config.h: a floating SS pin drops refreshes
  dmd.selectFont(Arial_Black_16);
  dmd.clearScreen(true);
  // Below the dust sensor task, above loop().
  xTaskCreatePinnedToCore(scanTask, "dmd", 2048, nullptr, 2, nullptr, 1);
}

void Display::show(const char* text) {
  strlcpy(nextText, text, sizeof(nextText));
}

void Display::update() {
  if (millis() - lastStepMs < DISPLAY_SCROLL_MS) return;
  lastStepMs = millis();

  // stepMarquee() returns true once the text has scrolled fully off-screen.
  if (scrolling && !dmd.stepMarquee(-1, 0)) return;
  if (nextText[0] == '\0') return;
  dmd.clearScreen(true);
  dmd.drawMarquee(nextText, strlen(nextText), DMD_PIXELS_ACROSS - 1, 0);
  scrolling = true;
}
