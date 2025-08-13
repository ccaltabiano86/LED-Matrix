#include <FastLED.h>

#define LED_PIN     2
#define WIDTH       64
#define HEIGHT      8
#define NUM_LEDS    (WIDTH * HEIGHT)
#define BRIGHTNESS  5

CRGB leds[NUM_LEDS];

// Helper function for serpentine index mapping
int getSerpentineIndex(int x, int y) {
  if (x % 2 == 0) {
    // Even columns: top to bottom
    return x * HEIGHT + y;
  } else {
    // Odd columns: bottom to top
    return x * HEIGHT + (HEIGHT - 1 - y);
  }
}

void setup() {
    delay(1000); // Give time for the Serial Monitor to open

    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(BRIGHTNESS);  // 👈 Lower brightness to save power or reduce heat
    FastLED.clear();
    FastLED.show();
}

void loop() {
    // Flash sequence before checkered flag
    fill_solid(leds, NUM_LEDS, CRGB::Blue);
    FastLED.show();
    delay(1000);

    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    delay(1000);

    fill_solid(leds, NUM_LEDS, CRGB::Red);
    FastLED.show();
    delay(3000);
    
    fill_solid(leds, NUM_LEDS, CRGB::Orange);
    FastLED.show();
    delay(3000);
    
    
    fill_solid(leds, NUM_LEDS, CRGB::Green);
    FastLED.show();
    delay(3000);
    
    // 🏁 flag animation 
    while (true) {
        for (int frame = 0; frame < 2; frame++) {

            for (int y = 0; y < HEIGHT; y++) {
                for (int x = 0; x < WIDTH; x++) {
                    int index = getSerpentineIndex(x, y);  // 🐍 use serpentine indexing!
                    
                    if ((((x / 4) + (y / 4) + frame) % 2) == 0) {
                        leds[index] = CRGB::Black;
                    } else {
                        leds[index] = CRGB::White;
                    }
                }
            }
            FastLED.show();
            delay(600);
        }
    }
}