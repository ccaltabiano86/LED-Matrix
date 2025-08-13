#include <FastLED.h>

#define LED_PIN     2
#define WIDTH       64
#define HEIGHT      8
#define NUM_LEDS    (WIDTH * HEIGHT)
#define BRIGHTNESS  5

#define BTN1_PIN    4       // Start sequence
#define BTN2_PIN    5       // Flag animation
#define BTN3_PIN    3       // Emergency stop (INT1 on Pro Mini)

#define DEBOUNCE_MS         200
#define HOLD_TO_RESET_MS    2000
#define EMERGENCY_COOLDOWN_MS 5000

CRGB leds[NUM_LEDS];

volatile bool emergencyActive = false;
bool isSequenceRunning = false;
unsigned long lastBtnPressTime = 0;
unsigned long lastEmergencyCleared = 0;

// 🐍 Serpentine mapping
int getSerpentineIndex(int x, int y) {
  return (x % 2 == 0) ? (x * HEIGHT + y) : (x * HEIGHT + (HEIGHT - 1 - y));
}

void setup() {
  delay(1000);
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear();
  FastLED.show();

  pinMode(BTN1_PIN, INPUT_PULLUP);
  pinMode(BTN2_PIN, INPUT_PULLUP);
  pinMode(BTN3_PIN, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(BTN3_PIN), emergencyISR, FALLING);
}

void loop() {
  unsigned long now = millis();

  // 🚨 Emergency Mode Handling
  if (emergencyActive) {
    fill_solid(leds, NUM_LEDS, CRGB::Red);
    FastLED.show();

    // Wait for BTN3 to be held to clear emergency
    if (digitalRead(BTN3_PIN) == LOW) {
      unsigned long holdStart = millis();
      while (digitalRead(BTN3_PIN) == LOW) {
        if (millis() - holdStart > HOLD_TO_RESET_MS) {
          emergencyActive = false;
          lastEmergencyCleared = millis();  // ⏱ Cooldown start
          FastLED.clear();
          FastLED.show();
          return;
        }
      }
    }
    return;
  }

  if (isSequenceRunning) return;

  // 🚦 Race sequence
  if (digitalRead(BTN1_PIN) == LOW && now - lastBtnPressTime > DEBOUNCE_MS) {
    lastBtnPressTime = now;
    isSequenceRunning = true;
    runRaceSequence();
    isSequenceRunning = false;
  }

  // 🏁 Checkered flag animation
  else if (digitalRead(BTN2_PIN) == LOW && now - lastBtnPressTime > DEBOUNCE_MS) {
    lastBtnPressTime = now;
    isSequenceRunning = true;
    runCheckeredFlag();
    isSequenceRunning = false;
  }
}

// ISR for emergency stop
void emergencyISR() {
  // Prevent re-triggering during cooldown
  if (millis() - lastEmergencyCleared > EMERGENCY_COOLDOWN_MS) {
    emergencyActive = true;
  }
}

// 🚦 Red → Orange → Green sequence
void runRaceSequence() {
  fill_solid(leds, NUM_LEDS, CRGB::Red);
  FastLED.show();
  delay(1500);
  if (emergencyActive) return;

  fill_solid(leds, NUM_LEDS, CRGB::Orange);
  FastLED.show();
  delay(1500);
  if (emergencyActive) return;

  fill_solid(leds, NUM_LEDS, CRGB::Green);
  FastLED.show();
  delay(3000);
  if (emergencyActive) return;

  FastLED.clear();
  FastLED.show();
}

// 🏁 Checkered flag animation
void runCheckeredFlag() {
  for (int i = 0; i < 10 && !emergencyActive; i++) {
    for (int frame = 0; frame < 2 && !emergencyActive; frame++) {
      for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
          int index = getSerpentineIndex(x, y);
          leds[index] = ((((x / 4) + (y / 4) + frame) % 2) == 0) ? CRGB::Black : CRGB::White;
        }
      }
      FastLED.show();
      delay(600);
    }
  }
  FastLED.clear();
  FastLED.show();
}
