#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>

// ---- ESP32 Access Point ----
const char* apSSID = "ESP32-Control";
const char* apPassword = "Password123";

// ---- Local WiFi ----
const char* wifiSSID = "HOGWARTS";
const char* wifiPassword = "karinak1990";

WebServer server(80);

// ---- LED strip config ----
#define LED_PIN     5
#define NUM_LEDS    256
#define LED_SEQ     64
#define BRIGHTNESS  100  // 0-255

#define STATUS_LED_PIN 15

// ---- Button config ----
#define START_BUTTON_PIN 6
#define STOP_BUTTON_PIN  7
const unsigned long DEBOUNCE_MS = 50;

Adafruit_NeoPixel np(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// forward-declare Button so Arduino's automatic prototype generation compiles
struct Button;

void fill(uint8_t r, uint8_t g, uint8_t b) {
  uint32_t c = np.Color(r, g, b);
  for (int i = 0; i < NUM_LEDS; i++) {
    np.setPixelColor(i, c);
  }
  np.show();
}

void clearAll() {
  fill(0, 0, 0);
}

// ---- Status LED blink (non-blocking heartbeat) ----
unsigned long lastBlink = 0;
bool statusLedState = false;
const unsigned long blinkInterval = 500;

void updateStatusLed() {
  if (millis() - lastBlink >= blinkInterval) {
    lastBlink = millis();
    statusLedState = !statusLedState;
    digitalWrite(STATUS_LED_PIN, statusLedState);
  }
}

// ---- Button debouncing ----
struct Button {
  uint8_t pin;
  bool lastReading;
  bool stableState;
  unsigned long lastChangeTime;
};

Button startBtn = {START_BUTTON_PIN, HIGH, HIGH, 0};
Button stopBtn  = {STOP_BUTTON_PIN,  HIGH, HIGH, 0};

// returns true exactly once, on the press edge (HIGH -> LOW, since INPUT_PULLUP)
bool checkPressed(Button &b) {
  bool reading = digitalRead(b.pin);
  if (reading != b.lastReading) {
    b.lastChangeTime = millis();
  }
  bool pressedEdge = false;
  if ((millis() - b.lastChangeTime) > DEBOUNCE_MS) {
    if (reading != b.stableState) {
      bool previousStable = b.stableState;
      b.stableState = reading;
      if (previousStable == HIGH && b.stableState == LOW) {
        pressedEdge = true;
      }
    }
  }
  b.lastReading = reading;
  return pressedEdge;
}

// ---- Sequence state machine ----
enum SeqState {
  SEQ_IDLE,          // off, waiting for Start
  SEQ_RED_ALL_ON,
  SEQ_RED_ALL_OFF,
  SEQ_ORANGE_SEG1,
  SEQ_ORANGE_SEG2,
  SEQ_ORANGE_SEG3,
  SEQ_GREEN_ALL,
  SEQ_DONE,          // sequence finished, LEDs off, waiting for Start
  SEQ_STOPPED_RED    // forced solid red after Stop, waiting for Start
};

void enterState(SeqState newState);
void startSequence();
void stopSequence();
void updateSequence();

SeqState seqState = SEQ_IDLE;
unsigned long stateEnteredAt = 0;
uint8_t orangeSegmentIndex = 0;
bool redOffIsInitial = false;

const unsigned long RED_ON_MS     = 3000; // 3s
const unsigned long RED_OFF_MS    = 1000; // 1s
const unsigned long ORANGE_ON_MS  = 1000; // 1s per 40-LED segment
const unsigned long GREEN_ON_MS   = 3000; // 3s

void enterState(SeqState newState) {
  seqState = newState;
  stateEnteredAt = millis();

  switch (seqState) {
    case SEQ_IDLE:
    case SEQ_DONE:
      clearAll();
      break;
    case SEQ_RED_ALL_ON:
      fill(255, 0, 0); // red
      break;
    case SEQ_RED_ALL_OFF:
      clearAll();
      break;
    case SEQ_ORANGE_SEG1:
    case SEQ_ORANGE_SEG2:
    case SEQ_ORANGE_SEG3: {
      // clearAll();
      int start = (orangeSegmentIndex) * LED_SEQ;
      int end = start + LED_SEQ;
      if (start < 0) start = 0;
      if (end > NUM_LEDS) end = NUM_LEDS;
      uint32_t c = np.Color(255, 90, 0);
      for (int i = start; i < end; i++) np.setPixelColor(i, c);
      np.show();
      break;
    }
    case SEQ_GREEN_ALL:
      fill(0, 255, 0); // green
      break;
    case SEQ_STOPPED_RED:
      fill(255, 0, 0); // red
      break;
  }
}

void startSequence() {
  orangeSegmentIndex = 0;
  redOffIsInitial = true;
  enterState(SEQ_RED_ALL_OFF);
}

void stopSequence() {
  enterState(SEQ_STOPPED_RED);
}

void updateSequence() {
  unsigned long elapsed = millis() - stateEnteredAt;

  switch (seqState) {
    case SEQ_RED_ALL_ON:
      if (elapsed >= RED_ON_MS) {
        redOffIsInitial = false;
        enterState(SEQ_RED_ALL_OFF);
      }
      break;

    case SEQ_RED_ALL_OFF:
      if (elapsed >= RED_OFF_MS) {
        if (redOffIsInitial) {
          // initial off before red: go to red on
          redOffIsInitial = false;
          enterState(SEQ_RED_ALL_ON);
        } else {
          // regular off after red: proceed to orange segments
          orangeSegmentIndex = 0;
          enterState(SEQ_ORANGE_SEG1);
        }
      }
      break;

    case SEQ_ORANGE_SEG1:
      if (elapsed >= ORANGE_ON_MS) {
        orangeSegmentIndex = 1;
        enterState(SEQ_ORANGE_SEG2);
      }
      break;

    case SEQ_ORANGE_SEG2:
      if (elapsed >= ORANGE_ON_MS) {
        orangeSegmentIndex = 2;
        enterState(SEQ_ORANGE_SEG3);
      }
      break;

    case SEQ_ORANGE_SEG3:
      if (elapsed >= ORANGE_ON_MS) {
        enterState(SEQ_GREEN_ALL);
      }
      break;

    case SEQ_GREEN_ALL:
      if (elapsed >= GREEN_ON_MS) enterState(SEQ_DONE);
      break;

    case SEQ_IDLE:
    case SEQ_DONE:
    case SEQ_STOPPED_RED:
      // nothing to do, waiting for a button press
      break;
  }
}

void handleRoot()
{
    server.send(200, "text/html", R"rawliteral(
<!DOCTYPE html>
<html>

<head>

<meta name="viewport" content="width=device-width, initial-scale=1">

<style>

body {
    background: linear-gradient(180deg, #111318 0%, #202124 100%);
    color: #f8fafc;
    font-family: Arial, Helvetica, sans-serif;
    text-align: center;
    margin: 0;
    min-height: 100vh;
    display: flex;
    align-items: center;
    justify-content: center;
}

.panel {
    width: min(92vw, 420px);
    background: rgba(255, 255, 255, 0.05);
    border: 1px solid rgba(255, 255, 255, 0.08);
    border-radius: 20px;
    padding: 26px 20px 18px;
    box-shadow: 0 14px 40px rgba(0, 0, 0, 0.35);
}

h2 {
    margin: 0 0 18px;
    font-size: 28px;
    letter-spacing: 0.5px;
}

button {
    width: 100%;
    height: 84px;
    font-size: 22px;
    font-weight: 700;
    margin: 12px 0;
    border: none;
    border-radius: 16px;
    cursor: pointer;
    color: #0f172a;
    background: linear-gradient(180deg, #f8fafc 0%, #cbd5e1 100%);
    box-shadow: 0 8px 18px rgba(0, 0, 0, 0.25);
    transition: transform 0.08s ease, background 0.2s ease, box-shadow 0.2s ease, opacity 0.2s ease;
}

button:hover {
    filter: brightness(1.05);
}

button:active {
    transform: scale(0.98);
}

button.pressed {
    background: linear-gradient(180deg, #4ade80 0%, #16a34a 100%);
    color: white;
    box-shadow: 0 8px 24px rgba(34, 197, 94, 0.45);
}

button:disabled {
    cursor: wait;
    opacity: 0.9;
}

</style>

<script>

function press(id) {
    const button = document.querySelector(`[data-id="${id}"]`);
    if (!button || button.disabled) {
        return;
    }

    button.disabled = true;
    button.classList.add('pressed');

    fetch("/pulse?id=" + id)
        .then(response => response.text())
        .then(data => console.log(data))
        .catch(error => console.error(error))
        .finally(() => {
            setTimeout(() => {
                button.classList.remove('pressed');
                button.disabled = false;
            }, 1000);
        });
}

</script>

</head>

<body>

<div class="panel">
    <h2>ESP32 Remote Buttons</h2>

    <button data-id="0" onclick="press(0)">START</button>
    <button data-id="1" onclick="press(1)">STOP</button>
    <button data-id="2" onclick="press(2)">SPARE</button>
</div>

</body>
</html>
)rawliteral");
}

void handlePulse()
{
    if (!server.hasArg("id"))
    {
        server.send(400, "text/plain", "Missing ID");
        return;
    }

    int id = server.arg("id").toInt();

    switch (id) {
      case 0:
        startSequence();
        server.send(200, "text/plain", "START");
        return;
      case 1:
        stopSequence();
        server.send(200, "text/plain", "STOP");
        return;
      case 2:
        server.send(200, "text/plain", "SPARE");
        return;
      default:
        server.send(400, "text/plain", "Invalid ID");
        return;
    }
}

void startWebServer()
{
  server.on("/", HTTP_GET, handleRoot);
  server.on("/pulse", HTTP_GET, handlePulse);
  server.onNotFound([]() {
    server.send(404, "text/plain", "Not found");
  });
  server.begin();
  Serial.println("Web server started on port 80");
}

void connectWiFi()
{
  Serial.println();
  Serial.println("Connecting to WiFi station...");

  WiFi.mode(WIFI_MODE_STA);
  WiFi.begin(wifiSSID, wifiPassword);

  unsigned long startAttempt = millis();
  while (millis() - startAttempt < 10000) {
    if (WiFi.status() == WL_CONNECTED) {
      break;
    }
    delay(250);
    Serial.print('.');
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("Connected to WiFi network as station");
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    if (MDNS.begin("esp32led")) {
      Serial.println("mDNS responder started at esp32led.local");
    } else {
      Serial.println("mDNS responder failed");
    }
  } else {
    Serial.println();
    Serial.println("Station connection failed, starting access point...");
    WiFi.mode(WIFI_MODE_AP);
    WiFi.softAP(apSSID, apPassword);
    IPAddress apIP = WiFi.softAPIP();
    Serial.print("AP SSID: ");
    Serial.println(apSSID);
    Serial.print("AP IP address: ");
    Serial.println(apIP);
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println("ESP32 LED Matrix starting...");

  np.begin();
  np.setBrightness(BRIGHTNESS);
  np.show();

  pinMode(STATUS_LED_PIN, OUTPUT);
  pinMode(START_BUTTON_PIN, INPUT_PULLUP);
  pinMode(STOP_BUTTON_PIN, INPUT_PULLUP);

  enterState(SEQ_IDLE);

  connectWiFi();
  startWebServer();
}

void loop() {
  updateStatusLed();
  server.handleClient();

  if (checkPressed(startBtn)) {
    startSequence();
  }
  if (checkPressed(stopBtn)) {
    stopSequence();
  }

  updateSequence();
}
