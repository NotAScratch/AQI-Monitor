#pragma once

// P10 (32x16) LED panel showing a scrolling message.
class Display {
public:
  void begin();
  // Sets the message; it takes over when the current one has scrolled off.
  void show(const char* text);
  // Advances the marquee. Call from loop() as often as possible.
  void update();
};
