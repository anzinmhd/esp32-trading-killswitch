// ============================================================
//  TRADING KILL SWITCH — Phase 1 (ESP32 Final Build)
//  Converted from Arduino UNO → ESP32
//  Phase 2: WiFi + Upstox API + Telegram alert
// ============================================================

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ── Pin Definitions (ESP32 38-pin) ───────────────────────────
#define PIN_SWITCH      4   // Lever switch (10kΩ pull-up to 3V3)
#define PIN_LED_RED    15   // PWM — Bi-color LED red leg
#define PIN_LED_GREEN  16   // PWM — Bi-color LED green leg
#define PIN_BUZZER     18   // Active buzzer I/O
#define OLED_SDA       21   // I2C SDA
#define OLED_SCL       22   // I2C SCL

// ── ESP32 PWM Config ─────────────────────────────────────────
// ESP32 doesn't use analogWrite — uses LEDC peripheral instead
#define LED_FREQ     5000   // 5kHz PWM frequency
#define LED_RES         8   // 8-bit resolution (0–255)

// ── OLED Config ──────────────────────────────────────────────
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64
#define OLED_RESET     -1
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ── Kill Switch Settings ─────────────────────────────────────
#define HOLD_SECONDS    3

// ── State Machine ────────────────────────────────────────────
enum State { ARMED, COUNTDOWN, EXECUTING, KILLED };
State currentState = ARMED;

unsigned long countdownStart      = 0;
int           lastCountdownSecond = -1;

// ── Forward Declarations ─────────────────────────────────────
void executeKill();
void showArmed();
void showCountdown(int secondsLeft);
void showExecuting();
void showKilled();
void showResetting();
void setLED(int redStrength, int greenStrength);
void beep(int times, int onMs = 200, int offMs = 150);
void drawHeader(const __FlashStringHelper* title);


// ============================================================
//  SETUP
// ============================================================
void setup() {
  Serial.begin(115200);  // ESP32 uses 115200
  Serial.println();
  Serial.println(F("===================================="));
  Serial.println(F("  TRADING KILL SWITCH - BOOTING    "));
  Serial.println(F("         ESP32 Phase 1              "));
  Serial.println(F("===================================="));

  // ── GPIO setup ───────────────────────────────────────────
  pinMode(PIN_SWITCH,  INPUT);   // external 10kΩ pull-up to 3V3
  pinMode(PIN_BUZZER,  OUTPUT);
  digitalWrite(PIN_BUZZER, HIGH); // active LOW buzzer — HIGH = silent

  // ── ESP32 PWM setup for LED ───────────────────────────────
  // ESP32 requires LEDC setup instead of analogWrite
 ledcAttach(PIN_LED_RED,   LED_FREQ, LED_RES);
 ledcAttach(PIN_LED_GREEN, LED_FREQ, LED_RES);
  setLED(0, 0); // all off at boot

  // ── I2C + OLED ───────────────────────────────────────────
  Wire.begin(OLED_SDA, OLED_SCL);  // ESP32 needs explicit pins
  Wire.setClock(100000);            // stable 100kHz

  // Retry OLED init up to 5 times
  bool oledOK = false;
  for (int i = 0; i < 5; i++) {
    if (display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
      oledOK = true;
      break;
    }
    delay(200);
  }
  if (!oledOK) {
    Serial.println(F("ERROR: OLED not found! Check SDA/SCL wiring."));
    while (true);
  }

  display.setTextColor(SSD1306_WHITE, SSD1306_BLACK); // prevents ghost text

  // Boot splash
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(16, 20);
  display.print(F("KILL SWITCH v1.0"));
  display.setCursor(24, 40);
  display.print(F("Initialising..."));
  display.display();
  delay(1500);

  beep(1, 80);

  setLED(0, 255);  // Green = ARMED
  showArmed();

  Serial.println(F("STATUS: ARMED — Waiting for switch"));
}


// ============================================================
//  MAIN LOOP
// ============================================================
void loop() {
  int switchVal = digitalRead(PIN_SWITCH);

  switch (currentState) {

    // ── ARMED ───────────────────────────────────────────────
    case ARMED:
      if (switchVal == LOW) {
        delay(100); // debounce
        if (digitalRead(PIN_SWITCH) == HIGH) break;

        Serial.println(F("Switch ON — starting countdown..."));
        countdownStart      = millis();
        lastCountdownSecond = HOLD_SECONDS;
        currentState        = COUNTDOWN;
        setLED(255, 150);  // Yellow (R full + G dimmed)
      }
      break;

    // ── COUNTDOWN ───────────────────────────────────────────
    case COUNTDOWN:
      if (switchVal == HIGH) {
        delay(100); // debounce
        if (digitalRead(PIN_SWITCH) == LOW) break;

        Serial.println(F("Released early — ABORTED"));
        currentState = ARMED;
        setLED(0, 255);  // Back to green
        showArmed();
        break;
      }
      {
        unsigned long elapsed = millis() - countdownStart;
        int secondsLeft       = HOLD_SECONDS - (int)(elapsed / 1000);
        secondsLeft           = constrain(secondsLeft, 0, HOLD_SECONDS);

        if (secondsLeft != lastCountdownSecond) {
          lastCountdownSecond = secondsLeft;
          showCountdown(secondsLeft);
          Serial.print(F("Countdown: "));
          Serial.println(secondsLeft);
        }

        if (elapsed >= (unsigned long)(HOLD_SECONDS * 1000)) {
          currentState = EXECUTING;
          executeKill();
        }
      }
      break;

    // ── EXECUTING ───────────────────────────────────────────
    case EXECUTING:
      break;

    // ── KILLED ──────────────────────────────────────────────
    case KILLED:
      if (switchVal == HIGH) {
        delay(100);
        if (digitalRead(PIN_SWITCH) == LOW) break;

        Serial.println(F("Switch OFF — resetting to ARMED"));
        showResetting();
        delay(800);
        setLED(0, 255);  // Green
        currentState = ARMED;
        showArmed();
        Serial.println(F("STATUS: ARMED — Waiting for switch"));
      }
      break;
  }

  delay(20);
}


// ============================================================
//  KILL SEQUENCE
//  Phase 1: simulated with delays
//  Phase 2: replace delays with real Upstox HTTPS API calls
// ============================================================
void executeKill() {
  Serial.println(F(""));
  Serial.println(F("╔══════════════════════════════════╗"));
  Serial.println(F("║    KILL SEQUENCE TRIGGERED        ║"));
  Serial.println(F("╚══════════════════════════════════╝"));

  setLED(255, 0);  // Solid Red = executing
  showExecuting();

  // Step 1 — Cancel all pending orders
  // <<< Phase 2: replace with Upstox DELETE /v2/orders
  Serial.println(F("Step 1: Cancelling all orders..."));
  delay(1200);

  // Step 2 — Close all open positions
  // <<< Phase 2: replace with Upstox POST /v2/order/place (opposing)
  Serial.println(F("Step 2: Closing all positions..."));
  delay(1200);

  // Step 3 — Send Telegram alert
  // <<< Phase 2: replace with Telegram Bot API call
  Serial.println(F("Step 3: Sending alert..."));

  Serial.println(F("╔══════════════════════════════════╗"));
  Serial.println(F("║      KILL SEQUENCE COMPLETE       ║"));
  Serial.println(F("║  All orders cancelled             ║"));
  Serial.println(F("║  All positions closed             ║"));
  Serial.println(F("║  Flip switch back to RESET        ║"));
  Serial.println(F("╚══════════════════════════════════╝"));

  showKilled();
  beep(3, 300, 200);

  setLED(255, 0);  // Red stays ON
  currentState = KILLED;
}


// ============================================================
//  OLED SCREENS
// ============================================================

void drawHeader(const __FlashStringHelper* title) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
  display.setTextSize(1);
  display.setCursor(8, 0);
  display.print(title);
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
}

void showArmed() {
  drawHeader(F("TRADING KILL SWITCH"));
  display.setTextSize(2);
  display.setCursor(34, 20);
  display.print(F("ARMED"));
  display.setTextSize(1);
  display.setCursor(8, 50);
  display.print(F("Flip switch to kill"));
  display.display();
}

void showCountdown(int secondsLeft) {
  drawHeader(F("TRADING KILL SWITCH"));
  display.setTextSize(1);
  display.setCursor(14, 14);
  display.print(F("CONFIRM HOLDING..."));
  display.setTextSize(3);
  display.setCursor(56, 26);
  display.print(secondsLeft);
  int barWidth = map(HOLD_SECONDS - secondsLeft, 0, HOLD_SECONDS, 0, 126);
  display.drawRect(0, 56, 128, 8, SSD1306_WHITE);
  display.fillRect(1, 57, barWidth, 6, SSD1306_WHITE);
  display.display();
}

void showExecuting() {
  drawHeader(F("TRADING KILL SWITCH"));
  display.setTextSize(1);
  display.setCursor(4, 16);
  display.print(F("PROCESSING EXECUTION"));
  display.setCursor(0, 34);
  display.print(F("> Cancelling Orders..."));
  display.setCursor(0, 48);
  display.print(F("> Closing Positions..."));
  display.display();
}

void showKilled() {
  display.clearDisplay();
  display.drawRect(0, 0, 128, 64, SSD1306_WHITE);
  display.drawRect(2, 2, 124, 60, SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(28, 16);
  display.print(F("KILLED!"));
  display.setTextSize(1);
  display.setCursor(4, 48);
  display.print(F("    Flip to reset!"));
  display.display();
}

void showResetting() {
  drawHeader(F("TRADING KILL SWITCH"));
  display.setTextSize(2);
  display.setCursor(34, 20);
  display.print(F("RESET"));
  display.setTextSize(1);
  display.setCursor(32, 50);
  display.print(F("Rearming..."));
  display.display();
}


// ============================================================
//  HARDWARE HELPERS
// ============================================================

// ESP32 PWM colour mixing via LEDC
void setLED(int redStrength, int greenStrength) {
  ledcWrite(PIN_LED_RED,   redStrength);
  ledcWrite(PIN_LED_GREEN, greenStrength);
}

// Active LOW buzzer — LOW = ON, HIGH = OFF
void beep(int times, int onMs, int offMs) {
  for (int i = 0; i < times; i++) {
    digitalWrite(PIN_BUZZER, LOW);   // ON
    delay(onMs);
    digitalWrite(PIN_BUZZER, HIGH);  // OFF
    if (i < times - 1) delay(offMs);
  }
}
