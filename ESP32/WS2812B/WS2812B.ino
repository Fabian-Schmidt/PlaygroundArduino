#define FASTLED_INTERNAL
#include "FastLED.h"

#define LED_PIN     17
#define NUM_LEDS    1
#define CALIBRATION_TEMPERATURE TypicalLEDStrip
#define MAX_BRIGHTNESS 255 // 0-255

CRGB leds[NUM_LEDS];

void setupLEDs() {
  // clear led color buffer to black
  memset(leds, 0, NUM_LEDS * sizeof(struct CRGB));
  FastLED.addLeds<WS2812, LED_PIN>(leds, 0, NUM_LEDS);
  FastLED.setCorrection(CALIBRATION_TEMPERATURE);
  FastLED.setTemperature(CALIBRATION_TEMPERATURE);
  FastLED.setBrightness(MAX_BRIGHTNESS);
  FastLED.setDither(DISABLE_DITHER);
}

void initialLEDflash() {
  LEDS.showColor(CRGB(127, 0, 0));
  delay(1333);
  LEDS.showColor(CRGB(0, 127, 0));
  delay(1333);
  LEDS.showColor(CRGB(0, 0, 127));
  delay(1333);
  LEDS.showColor(CRGB(0, 0, 0));
}

void setup() {
  setupLEDs();
  initialLEDflash();
}

void loop() {
  leds[0] = CRGB::White; FastLED.show(); delay(3000);
  leds[0] = CRGB::Black; FastLED.show(); delay(3000);

  initialLEDflash();
  delay(3000);
}
