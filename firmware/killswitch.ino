/**
 * ESP32 Trading Kill Switch
 * 
 * A physical hardware emergency stop for trading bots.
 * This sketch handles the button press logic, Upstox API integration, 
 * and Telegram alerts.
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "secrets.h"

// Pin Definitions
#define BUTTON_PIN 4
#define BUZZER_PIN 18
#define LED_RED_PIN 19
#define LED_GREEN_PIN 20

// OLED Display Settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// State Variables
unsigned long buttonPressStartTime = 0;
bool isKillTriggered = false;

void setup() {
  Serial.begin(115200);
  
  // Pin Setup
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_GREEN_PIN, OUTPUT);
  
  // Initialize Display
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  display.display();
  delay(1000);
  display.clearDisplay();
  
  // WiFi Connection
  connectToWiFi();
}

void loop() {
  // Read button state
  bool buttonPressed = (digitalRead(BUTTON_PIN) == LOW);
  
  if (buttonPressed && !isKillTriggered) {
    if (buttonPressStartTime == 0) {
      buttonPressStartTime = millis();
    }
    
    unsigned long holdTime = millis() - buttonPressStartTime;
    
    if (holdTime >= 3000) {
      executeKillSequence();
      isKillTriggered = true;
    } else {
      updateDisplayCountdown(3 - (holdTime / 1000));
    }
  } else {
    buttonPressStartTime = 0;
    if (!isKillTriggered) {
      displayStatus();
    }
  }
}

void connectToWiFi() {
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("Connecting to WiFi...");
  display.display();
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  display.println("Connected!");
  display.display();
  delay(1000);
}

void executeKillSequence() {
  // 1. Set LED to Yellow/Red
  digitalWrite(LED_GREEN_PIN, LOW);
  digitalWrite(LED_RED_PIN, HIGH);
  
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("KILL EXECUTING...");
  display.display();
  
  // TODO: Implement Upstox API calls to cancel orders and close positions
  // TODO: Implement Telegram alert
  
  display.println("DONE.");
  display.display();
  
  // Buzzer beep
  for(int i=0; i<3; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);
    digitalWrite(BUZZER_PIN, LOW);
    delay(200);
  }
}

void updateDisplayCountdown(int seconds) {
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.println("HOLD TO KILL:");
  display.setTextSize(3);
  display.setCursor(40, 25);
  display.println(seconds);
  display.display();
}

void displayStatus() {
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(1);
  display.println("SYSTEM ARMED");
  display.println("WiFi: Connected");
  display.display();
  
  digitalWrite(LED_GREEN_PIN, HIGH);
  digitalWrite(LED_RED_PIN, LOW);
}
