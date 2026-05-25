// ============================================================
//  TRADING KILL SWITCH — Phase 2 (ESP32)
//  Features: WiFi + Web Dashboard + Telegram + Dual Target
//  Hardware: ESP32 38-pin + SSD1306 OLED + Bi-color LED
//            + Active Buzzer (active LOW) + Lever Switch
// ============================================================

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <Preferences.h>
#include "secrets.h"

// ── Pin Definitions ──────────────────────────────────────────
#define PIN_SWITCH      4
#define PIN_LED_RED    15
#define PIN_LED_GREEN  16
#define PIN_BUZZER     18
#define OLED_SDA       21
#define OLED_SCL       22

// ── LED PWM ──────────────────────────────────────────────────
#define LED_FREQ     5000
#define LED_RES         8

// ── OLED ─────────────────────────────────────────────────────
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64
#define OLED_RESET     -1
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ── Web Server ───────────────────────────────────────────────
WebServer server(80);

// ── State Machine ────────────────────────────────────────────
enum State { ARMED, COUNTDOWN, EXECUTING, KILLED };
State currentState = ARMED;

unsigned long countdownStart      = 0;
int           lastCountdownSecond = -1;
#define HOLD_SECONDS 3

// ── Target Toggles (web dashboard controls these) ────────────
bool targetAzure = true;   // Azure VM — ON by default
bool targetLocal  = false;  // Local Mac — OFF by default

// ── Persistent Storage ───────────────────────────────────────
Preferences prefs;

// ── Kill Results ─────────────────────────────────────────────
String lastKillTime  = "Never";
String azureResult   = "--";
String localResult   = "--";
String telegramResult = "--";

// ── Forward Declarations ─────────────────────────────────────
void executeKill();
void connectWiFi();
void setupWebServer();
bool sendKillSignal(const char* url, String &result);
bool sendTelegram(String message);
void showArmed();
void showCountdown(int secondsLeft);
void showExecuting(String step);
void showKilled();
void showResetting();
void showWiFiConnecting();
void showWiFiConnected();
void drawHeader(const char* title);
void setLED(int r, int g);
void beep(int times, int onMs = 200, int offMs = 150);


// ============================================================
//  SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  Serial.println(F("\n===================================="));
  Serial.println(F("  TRADING KILL SWITCH v2.0 BOOTING "));
  Serial.println(F("  Phase 2: WiFi + Dashboard         "));
  Serial.println(F("===================================="));

  // ── GPIO ─────────────────────────────────────────────────
  pinMode(PIN_SWITCH,  INPUT);
  pinMode(PIN_BUZZER,  OUTPUT);
  digitalWrite(PIN_BUZZER, HIGH); // active LOW — HIGH = silent

  ledcAttach(PIN_LED_RED,   LED_FREQ, LED_RES);
  ledcAttach(PIN_LED_GREEN, LED_FREQ, LED_RES);
  setLED(0, 0);

  // ── OLED ─────────────────────────────────────────────────
  Wire.begin(OLED_SDA, OLED_SCL);
  Wire.setClock(100000);

  bool oledOK = false;
  for (int i = 0; i < 5; i++) {
    if (display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
      oledOK = true;
      break;
    }
    delay(200);
  }
  if (!oledOK) {
    Serial.println(F("ERROR: OLED not found!"));
    while (true);
  }
  display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);

  // ── Boot splash ──────────────────────────────────────────
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(16, 16);
  display.print(F("KILL SWITCH v2.0"));
  display.setCursor(28, 32);
  display.print(F("Phase 2: WiFi"));
  display.setCursor(20, 48);
  display.print(F("Booting..."));
  display.display();
  delay(1200);

  beep(1, 80);

  // ── Load saved toggle state from NVS ─────────────────────
  prefs.begin("killswitch", true); // true = read-only
  targetAzure = prefs.getBool("azure", true);  // default ON
  targetLocal  = prefs.getBool("local", false); // default OFF
  prefs.end();
  Serial.print(F("Loaded from NVS — Azure: "));
  Serial.print(targetAzure ? F("ON") : F("OFF"));
  Serial.print(F(" Local: "));
  Serial.println(targetLocal ? F("ON") : F("OFF"));

  // ── Connect WiFi ─────────────────────────────────────────
  connectWiFi();

  // ── Start web server ─────────────────────────────────────
  setupWebServer();

  // ── Ready ────────────────────────────────────────────────
  setLED(0, 255);
  showArmed();

  Serial.println(F("STATUS: ARMED — Waiting for switch"));
  Serial.print(F("Dashboard: http://"));
  Serial.println(WiFi.localIP());
}


// ============================================================
//  MAIN LOOP
// ============================================================
void loop() {
  // Handle web dashboard requests
  server.handleClient();

  // Auto-reconnect WiFi if dropped
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("WiFi lost — reconnecting..."));
    connectWiFi();
  }

  int switchVal = digitalRead(PIN_SWITCH);

  switch (currentState) {

    // ── ARMED ──────────────────────────────────────────────
    case ARMED:
      if (switchVal == LOW) {
        delay(100);
        if (digitalRead(PIN_SWITCH) == HIGH) break;
        Serial.println(F("Switch ON — starting countdown..."));
        countdownStart      = millis();
        lastCountdownSecond = HOLD_SECONDS;
        currentState        = COUNTDOWN;
        setLED(255, 150); // Yellow
      }
      break;

    // ── COUNTDOWN ──────────────────────────────────────────
    case COUNTDOWN:
      server.handleClient(); // keep dashboard alive during countdown

      if (switchVal == HIGH) {
        delay(100);
        if (digitalRead(PIN_SWITCH) == LOW) break;
        Serial.println(F("Released early — ABORTED"));
        currentState = ARMED;
        setLED(0, 255);
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

    // ── EXECUTING ──────────────────────────────────────────
    case EXECUTING:
      break;

    // ── KILLED ─────────────────────────────────────────────
    case KILLED:
      if (switchVal == HIGH) {
        delay(100);
        if (digitalRead(PIN_SWITCH) == LOW) break;
        Serial.println(F("Switch OFF — resetting to ARMED"));
        showResetting();
        delay(800);
        setLED(0, 255);
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
// ============================================================
void executeKill() {
  Serial.println(F("\n╔══════════════════════════════════╗"));
  Serial.println(F("║    KILL SEQUENCE TRIGGERED        ║"));
  Serial.println(F("╚══════════════════════════════════╝"));

  setLED(255, 0); // Red

  // Record timestamp
  lastKillTime = String(millis() / 1000) + "s uptime";
  azureResult  = "--";
  localResult  = "--";
  telegramResult = "--";

  // ── Step 1: Azure VM ───────────────────────────────────
  if (targetAzure) {
    showExecuting("Killing Azure VM...");
    Serial.println(F("Step 1: Sending kill to Azure VM..."));
    bool ok = sendKillSignal(AZURE_KILL_URL "?target=Azure%20VM", azureResult);
    Serial.print(F("Azure result: "));
    Serial.println(azureResult);
    if (ok) beep(1, 100); else beep(2, 50, 50);
  } else {
    azureResult = "Disabled";
    Serial.println(F("Step 1: Azure target disabled — skipping"));
  }

  // ── Step 2: Local Mac ──────────────────────────────────
  if (targetLocal) {
    showExecuting("Killing Local Mac...");
    Serial.println(F("Step 2: Sending kill to Local Mac..."));
    bool ok = sendKillSignal(LOCAL_KILL_URL "?target=Local%20Mac", localResult);
    Serial.print(F("Local result: "));
    Serial.println(localResult);
    if (ok) beep(1, 100); else beep(2, 50, 50);
  } else {
    localResult = "Disabled";
    Serial.println(F("Step 2: Local target disabled — skipping"));
  }

  // ── Step 3: Telegram ─────────────────────────────────────
  if (targetAzure && targetLocal) {
    telegramResult = "Via Both ✓";
  } else if (targetAzure) {
    telegramResult = "Via Azure ✓";
  } else if (targetLocal) {
    telegramResult = "Via Mac ✓";
  } else {
    telegramResult = "Disabled";
  }
  Serial.print(F("Step 3: Telegram status: "));
  Serial.println(telegramResult);

  // ── Done ───────────────────────────────────────────────
  Serial.println(F("╔══════════════════════════════════╗"));
  Serial.println(F("║      KILL SEQUENCE COMPLETE       ║"));
  Serial.println(F("╚══════════════════════════════════╝"));
  showExecuting("Sequence Complete!");
  
  delay(2000); 

  showKilled();
  beep(3, 300, 200);
  setLED(255, 0);
  currentState = KILLED;
}



// ============================================================
//  WiFi
// ============================================================
void connectWiFi() {
  showWiFiConnecting();
  Serial.print(F("Connecting to WiFi: "));
  Serial.println(WIFI_SSID);

  WiFi.disconnect(true);   // ← add this line
  delay(100);              // ← add this line
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(F("."));
    attempts++;
    // Blink yellow while connecting
    setLED(attempts % 2 == 0 ? 255 : 0, attempts % 2 == 0 ? 150 : 0);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.print(F("Connected! IP: "));
    Serial.println(WiFi.localIP());
    setLED(0, 255);
    showWiFiConnected();
    delay(2000);
  } else {
    Serial.println(F("WiFi FAILED — running offline"));
    // Still boot — kill switch works locally (LED + buzzer)
    // but won't reach API or Telegram
    display.clearDisplay();
    drawHeader("KILL SWITCH v2.0");
    display.setCursor(0, 16);
    display.print(F("WiFi FAILED"));
    display.setCursor(0, 28);
    display.print(F("Check SSID/password"));
    display.setCursor(0, 42);
    display.print(F("Running offline..."));
    display.display();
    setLED(255, 0);
    beep(3, 50, 50);
    delay(2000);
  }
}


// ============================================================
//  HTTP — Send kill signal to target server
// ============================================================
bool sendKillSignal(const char* url, String &result) {
  if (WiFi.status() != WL_CONNECTED) {
    result = "No WiFi";
    return false;
  }

  HTTPClient http;
  http.begin(url);
  http.setTimeout(3000); // 3 second timeout — don't hang if Mac is off
  http.addHeader("X-Kill-Token", KILL_SECRET);

  int code = http.POST("");
  if (code == 200) {
    result = "Killed ✓";
    http.end();
    return true;
  } else if (code < 0) {
    result = "Offline";
    http.end();
    return false;
  } else {
    result = "Err " + String(code);
    http.end();
    return false;
  }
}


// ============================================================
//  TELEGRAM — Send alert message
// ============================================================
bool sendTelegram(String message) {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure client;
  client.setInsecure(); // skip cert verification — fine for Telegram alerts

  HTTPClient http;
  String url = "https://api.telegram.org/bot";
  url += TELEGRAM_BOT_TOKEN;
  url += "/sendMessage";

  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");

  // Escape special chars in message for JSON
  message.replace("\"", "\\\"");
  message.replace("\n", "\\n");

  String payload = "{\"chat_id\":\"";
  payload += TELEGRAM_CHAT_ID;
  payload += "\",\"text\":\"";
  payload += message;
  payload += "\",\"parse_mode\":\"Markdown\"}";

  int code = http.POST(payload);
  http.end();

  return (code == 200);
}


// ============================================================
//  WEB DASHBOARD
// ============================================================
void setupWebServer() {

  // ── Main dashboard page ──────────────────────────────────
  server.on("/", HTTP_GET, []() {
    String stateColor = (currentState == KILLED) ? "#ef4444" : "#22c55e";
    String stateText  = (currentState == KILLED) ? "KILLED 🔴" : "ARMED 🟢";

    String html = R"rawhtml(
<!DOCTYPE html>
<html>
<head>
  <meta charset='UTF-8'>
  <meta name='viewport' content='width=device-width, initial-scale=1'>
  <title>Kill Switch Dashboard</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { background: #0f172a; color: #f1f5f9; font-family: -apple-system, sans-serif; padding: 20px; }
    h1 { font-size: 1.4rem; font-weight: 700; margin-bottom: 4px; }
    .sub { color: #94a3b8; font-size: 0.8rem; margin-bottom: 24px; }
    .card { background: #1e293b; border-radius: 12px; padding: 16px; margin-bottom: 16px; }
    .card h2 { font-size: 0.75rem; text-transform: uppercase; letter-spacing: 1px; color: #64748b; margin-bottom: 12px; }
    .status-row { display: flex; justify-content: space-between; align-items: center; padding: 8px 0; border-bottom: 1px solid #334155; }
    .status-row:last-child { border-bottom: none; }
    .status-label { font-size: 0.85rem; color: #94a3b8; }
    .status-badge { display: inline-block; padding: 4px 12px; border-radius: 20px; font-weight: 700; font-size: 0.85rem; }
    .badge-green { background: #166534; color: #86efac; }
    .badge-red   { background: #7f1d1d; color: #fca5a5; }
    .badge-gray  { background: #1e293b; color: #64748b; border: 1px solid #334155; }
    .badge-loading { background: #1e293b; color: #64748b; font-size: 0.75rem; }
    .row { display: flex; justify-content: space-between; align-items: center; padding: 10px 0; border-bottom: 1px solid #334155; }
    .row:last-child { border-bottom: none; }
    .row label { font-size: 0.95rem; }
    .row .result { font-size: 0.8rem; color: #94a3b8; }
    .toggle { position: relative; width: 52px; height: 28px; }
    .toggle input { opacity: 0; width: 0; height: 0; }
    .slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background: #475569; border-radius: 28px; transition: .3s; }
    .slider:before { position: absolute; content: ""; height: 22px; width: 22px; left: 3px; bottom: 3px; background: white; border-radius: 50%; transition: .3s; }
    input:checked + .slider { background: #22c55e; }
    input:checked + .slider:before { transform: translateX(24px); }
    .btn { width: 100%; padding: 14px; border: none; border-radius: 10px; font-size: 1rem; font-weight: 700; cursor: pointer; margin-top: 8px; }
    .btn-reset { background: #22c55e; color: white; }
    .btn-test  { background: #3b82f6; color: white; }
    .ip { color: #38bdf8; font-size: 0.75rem; margin-top: 6px; }
    .last-kill { color: #64748b; font-size: 0.78rem; margin-top: 4px; }
    .divider { height: 1px; background: #334155; margin: 4px 0; }
  </style>
</head>
<body>
  <h1>⛔ Kill Switch</h1>
  <p class='sub'>Trade-Lab Emergency Stop • Phase 2</p>

  <!-- ── System Status Card ── -->
  <div class='card'>
    <h2>System Status</h2>

    <div class='status-row'>
      <span class='status-label'>Kill Switch</span>
      <span class='status-badge )rawhtml";
    html += (currentState == KILLED) ? "badge-red'" : "badge-green'";
    html += ">";
    html += (currentState == KILLED) ? "KILLED 🔴" : "ARMED 🟢";
    html += R"rawhtml(</span>
    </div>

    <!-- ── Azure section ── -->
    <div id='azure-status-section'>
    <div class='status-row' style='margin-top:8px'>
      <span class='status-label' style='color:#38bdf8;font-size:0.7rem;text-transform:uppercase;letter-spacing:1px'>🌐 Azure VM</span>
    </div>
    <div class='status-row'>
      <span class='status-label'>Bot</span>
      <span class='status-badge badge-loading' id='azure-bot-status'>Checking...</span>
    </div>
    <div class='status-row'>
      <span class='status-label'>Kill Flag</span>
      <span class='status-badge badge-loading' id='azure-flag-status'>Checking...</span>
    </div>
    <div class='status-row'>
      <span class='status-label'>Kill Server</span>
      <span class='status-badge badge-loading' id='azure-server-status'>Checking...</span>
    </div>
    </div>

    <!-- ── Local Mac section (hidden when disabled) ── -->
    <div id='local-status-section' style='display:none'>
      <div class='status-row' style='margin-top:8px'>
        <span class='status-label' style='color:#a78bfa;font-size:0.7rem;text-transform:uppercase;letter-spacing:1px'>💻 Local Mac</span>
      </div>
      <div class='status-row'>
        <span class='status-label'>Bot</span>
        <span class='status-badge badge-loading' id='local-bot-status'>Checking...</span>
      </div>
      <div class='status-row'>
        <span class='status-label'>Kill Flag</span>
        <span class='status-badge badge-loading' id='local-flag-status'>Checking...</span>
      </div>
      <div class='status-row'>
        <span class='status-label'>Kill Server</span>
        <span class='status-badge badge-loading' id='local-server-status'>Checking...</span>
      </div>
    </div>

    <p class='last-kill'>Last kill: )rawhtml";
    html += lastKillTime;
    html += R"rawhtml(</p>
    <p class='ip'>Dashboard: http://)rawhtml";
    html += WiFi.localIP().toString();
    html += R"rawhtml(</p>
  </div>

  <!-- ── Kill Targets Card ── -->
  <div class='card'>
    <h2>Kill Targets</h2>
    <div class='row'>
      <div>
        <label>🌐 Azure VM</label>
        <div class='result' id='azure-result'>)rawhtml";
    html += azureResult;
    html += R"rawhtml(</div>
      </div>
      <label class='toggle'>
        <input type='checkbox' id='azure' )rawhtml";
    html += targetAzure ? "checked" : "";
    html += R"rawhtml( onchange="toggle('azure', this.checked)">
        <span class='slider'></span>
      </label>
    </div>
    <div class='row'>
      <div>
        <label>💻 Local Mac</label>
        <div class='result' id='local-result'>)rawhtml";
    html += localResult;
    html += R"rawhtml(</div>
      </div>
      <label class='toggle'>
        <input type='checkbox' id='local' )rawhtml";
    html += targetLocal ? "checked" : "";
    html += R"rawhtml( onchange="toggle('local', this.checked)">
        <span class='slider'></span>
      </label>
    </div>
  </div>

  <!-- ── Last Kill Results Card ── -->
  <div class='card'>
    <h2>Last Kill Results</h2>
    <div class='row'><label>Azure VM</label><span>)rawhtml";
    html += azureResult;
    html += R"rawhtml(</span></div>
    <div class='row'><label>Local Mac</label><span>)rawhtml";
    html += localResult;
    html += R"rawhtml(</span></div>
    <div class='row'><label>Telegram</label><span>)rawhtml";
    html += telegramResult;
    html += R"rawhtml(</span></div>
  </div>

  <button class='btn btn-reset' onclick="doReset()">✅ Reset (Re-arm Bot)</button>
  <button class='btn btn-test'  onclick="doTest()" style='margin-top:8px'>🔔 Test Telegram</button>

  <script>
    // ── Fetch Azure bot status asynchronously ──────────────
    function setBadge(id, status) {
      var el = document.getElementById(id);
      if (!el) return;
      var map = {
        'RUNNING':  ['badge-green', 'RUNNING 🟢'],
        'BLOCKED':  ['badge-red',   'BLOCKED 🔴'],
        'OFFLINE':  ['badge-gray',  'OFFLINE ⚫'],
        'ONLINE':   ['badge-green', 'ONLINE ✅'],
        'ACTIVE':   ['badge-red',   'ACTIVE ⚠️'],
        'INACTIVE': ['badge-green', 'INACTIVE ✅'],
        'DISABLED': ['badge-gray',  'DISABLED ⚫'],
        'ERROR':    ['badge-red',   'ERROR ❌'],
      };
      var entry = map[status] || ['badge-gray', status];
      el.className   = 'status-badge ' + entry[0];
      el.textContent = entry[1];
    }

    function fetchBotStatus() {
      fetch('/api/botstatus')
        .then(r => r.json())
        .then(data => {
          // ── Azure section ──────────────────────────────
          var azureSection = document.getElementById('azure-status-section');
          if (data.azure.enabled) {
            azureSection.style.display = 'block';
            setBadge('azure-bot-status',    data.azure.status);
            setBadge('azure-flag-status',   data.azure.killed ? 'ACTIVE' : 'INACTIVE');
            setBadge('azure-server-status', data.azure.reachable ? 'ONLINE' : 'OFFLINE');
          } else {
            azureSection.style.display = 'none';
          }

          // ── Local section ──────────────────────────────
          var localSection = document.getElementById('local-status-section');
          if (data.local.enabled) {
            localSection.style.display = 'block';
            setBadge('local-bot-status',    data.local.status);
            setBadge('local-flag-status',   data.local.killed ? 'ACTIVE' : 'INACTIVE');
            setBadge('local-server-status', data.local.reachable ? 'ONLINE' : 'OFFLINE');
          } else {
            localSection.style.display = 'none';
          }
        })
        .catch(() => {
          ['azure-bot-status','azure-flag-status','azure-server-status'].forEach(id => {
            setBadge(id, 'ERROR');
          });
        });
    }

    // ── Toggle target ──────────────────────────────────────
    function toggle(target, val) {
      fetch('/toggle?target=' + target + '&val=' + (val ? '1' : '0'));
    }

    // ── Reset ──────────────────────────────────────────────
    function doReset() {
      if (confirm('Reset kill flag and re-arm the bot?')) {
        fetch('/webreset', {method:'POST'})
          .then(() => location.reload());
      }
    }

    // ── Test Telegram ──────────────────────────────────────
    function doTest() {
      fetch('/test', {method:'POST'})
        .then(r => r.text())
        .then(t => alert(t));
    }

    // Fetch bot status on load (async — page shows instantly)
    fetchBotStatus();

    // Auto refresh every 30 seconds
    setTimeout(() => location.reload(), 30000);
  </script>
</body>
</html>
)rawhtml";

    server.send(200, "text/html", html);
  });

  // ── Toggle target on/off ─────────────────────────────────
  server.on("/toggle", HTTP_GET, []() {
    String target = server.arg("target");
    String val    = server.arg("val");
    bool on = (val == "1");

    if (target == "azure") {
      targetAzure = on;
      Serial.print(F("Azure target: "));
      Serial.println(on ? F("ON") : F("OFF"));
    } else if (target == "local") {
      targetLocal = on;
      Serial.print(F("Local target: "));
      Serial.println(on ? F("ON") : F("OFF"));
    }

    // ── Save to NVS immediately ───────────────────────────
    prefs.begin("killswitch", false); // false = read-write
    prefs.putBool("azure", targetAzure);
    prefs.putBool("local", targetLocal);
    prefs.end();
    Serial.println(F("Toggle state saved to NVS"));

    server.send(200, "text/plain", "ok");
  });

  // ── Web reset button ─────────────────────────────────────
  server.on("/webreset", HTTP_POST, []() {
    // Reset Azure
    if (targetAzure) {
      HTTPClient http;
      http.begin(AZURE_RESET_URL "?target=Azure%20VM");
      http.addHeader("X-Kill-Token", KILL_SECRET);
      http.POST("");
      http.end();
    }
    if (targetLocal) {
      HTTPClient http;
      http.begin(LOCAL_RESET_URL "?target=Local%20Mac");
      http.addHeader("X-Kill-Token", KILL_SECRET);
      http.setTimeout(2000);
      http.POST("");
      http.end();
    }
    // Reset OLED + LED
    if (currentState == KILLED) {
      currentState = ARMED;
      setLED(0, 255);
      showArmed();
      azureResult    = "--";
      localResult    = "--";
      telegramResult = "--";
    }
    server.send(200, "text/plain", "Reset sent");
    Serial.println(F("Web reset triggered"));
  });

  // ── Test Telegram ────────────────────────────────────────
  server.on("/test", HTTP_POST, []() {
    bool ok = sendTelegram(
      "🔔 *Kill Switch Test*\n\n"
      "Hardware kill switch is online and connected.\n"
      "Azure: " + String(targetAzure ? "Enabled" : "Disabled") + "\n"
      "Local: " + String(targetLocal ? "Enabled" : "Disabled")
    );
    server.send(200, "text/plain", ok ? "Telegram sent ✓" : "Telegram failed ✗");
  });

  // ── API status for external tools ───────────────────────
  server.on("/api/status", HTTP_GET, []() {
    String json = "{";
    json += "\"state\":\"" + String(currentState == KILLED ? "KILLED" : "ARMED") + "\",";
    json += "\"targetAzure\":" + String(targetAzure ? "true" : "false") + ",";
    json += "\"targetLocal\":" + String(targetLocal ? "true" : "false") + ",";
    json += "\"lastKill\":\"" + lastKillTime + "\",";
    json += "\"azure\":\"" + azureResult + "\",";
    json += "\"local\":\"" + localResult + "\",";
    json += "\"telegram\":\"" + telegramResult + "\",";
    json += "\"ip\":\"" + WiFi.localIP().toString() + "\"";
    json += "}";
    server.send(200, "application/json", json);
  });

  // ── Bot status — fetches live from Azure kill server ─────
  server.on("/api/botstatus", HTTP_GET, []() {
    // ── Azure status ─────────────────────────────────────
    bool azureReachable = false;
    bool azureKilled    = false;

    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(AZURE_HEALTH_URL);
      http.setTimeout(3000);
      int code = http.GET();
      if (code == 200) {
        azureReachable = true;
        String body = http.getString();
        azureKilled = body.indexOf("KILLED") >= 0;
      }
      http.end();
    }

    // ── Local Mac status ─────────────────────────────────
    bool localReachable = false;
    bool localKilled    = false;

    if (WiFi.status() == WL_CONNECTED && targetLocal) {
      HTTPClient http;
      http.begin(LOCAL_HEALTH_URL);
      http.setTimeout(2000); // shorter timeout — Mac might be off
      int code = http.GET();
      if (code == 200) {
        localReachable = true;
        String body = http.getString();
        localKilled = body.indexOf("KILLED") >= 0;
      }
      http.end();
    }

    // ── Build JSON response ──────────────────────────────
    String json = "{";

    // Azure
    json += "\"azure\":{";
    json += "\"enabled\":" + String(targetAzure ? "true" : "false") + ",";
    json += "\"reachable\":" + String(azureReachable ? "true" : "false") + ",";
    json += "\"killed\":" + String(azureKilled ? "true" : "false") + ",";
    json += "\"status\":\"" + String(!targetAzure ? "DISABLED" : !azureReachable ? "OFFLINE" : azureKilled ? "BLOCKED" : "RUNNING") + "\"";
    json += "},";

    // Local
    json += "\"local\":{";
    json += "\"enabled\":" + String(targetLocal ? "true" : "false") + ",";
    json += "\"reachable\":" + String(localReachable ? "true" : "false") + ",";
    json += "\"killed\":" + String(localKilled ? "true" : "false") + ",";
    json += "\"status\":\"" + String(!targetLocal ? "DISABLED" : !localReachable ? "OFFLINE" : localKilled ? "BLOCKED" : "RUNNING") + "\"";
    json += "}";

    json += "}";
    server.send(200, "application/json", json);
  });

  server.begin();
  Serial.println(F("Web server started on port 80"));
}


// ============================================================
//  OLED SCREENS
// ============================================================

void drawHeader(const char* title) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(title);
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
}

void showWiFiConnecting() {
  display.clearDisplay();
  drawHeader("KILL SWITCH v2.0");
  display.setCursor(0, 16);
  display.print(F("Connecting WiFi..."));
  display.setCursor(0, 28);
  display.print(WIFI_SSID);
  display.display();
}

void showWiFiConnected() {
  display.clearDisplay();
  drawHeader("KILL SWITCH v2.0");
  display.setCursor(0, 16);
  display.print(F("WiFi Connected!"));
  display.setCursor(0, 28);
  display.print(F("IP: "));
  display.print(WiFi.localIP());
  display.setCursor(0, 42);
  display.print(F("Open in browser:"));
  display.setCursor(0, 52);
  display.print(F("http://"));
  display.print(WiFi.localIP());
  display.display();
}

void showArmed() {
  display.clearDisplay();
  drawHeader("TRADING KILL SWITCH");
  display.setTextSize(2);
  display.setCursor(34, 18);
  display.print(F("ARMED"));
  display.setTextSize(1);
  // Show IP so you can open dashboard
  display.setCursor(0, 42);
  display.print(F("http://"));
  display.print(WiFi.localIP());
  display.setCursor(0, 54);
  display.print(F("Flip switch to kill"));
  display.display();
}

void showCountdown(int secondsLeft) {
  display.clearDisplay();
  drawHeader("TRADING KILL SWITCH");
  display.setTextSize(1);
  display.setCursor(14, 14);
  display.print(F("HOLD TO CONFIRM..."));
  display.setTextSize(3);
  display.setCursor(56, 26);
  display.print(secondsLeft);
  int barWidth = map(HOLD_SECONDS - secondsLeft, 0, HOLD_SECONDS, 0, 126);
  display.drawRect(0, 56, 128, 8, SSD1306_WHITE);
  display.fillRect(1, 57, barWidth, 6, SSD1306_WHITE);
  display.display();
}

void showExecuting(String step) {
  display.clearDisplay();
  drawHeader("KILL IN PROGRESS");
  display.setTextSize(1);
  display.setCursor(0, 14);
  display.print(F("Azure: "));
  display.print(azureResult);
  display.setCursor(0, 26);
  display.print(F("Local: "));
  display.print(localResult);
  display.setCursor(0, 38);
  display.print(F("TG:    "));
  display.print(telegramResult);
  display.setCursor(0, 52);
  display.print(step.substring(0, 21));
  display.display();
}

void showKilled() {
  display.clearDisplay();
  // Draw double borders
  display.drawRect(0, 0, 128, 64, SSD1306_WHITE);
  display.drawRect(2, 2, 124, 60, SSD1306_WHITE);
  
  // Header
  display.setTextSize(2);
  display.setCursor(28, 6);
  display.print(F("KILLED"));
  
  // Status lines (spaced tightly to fit 4 rows)
  display.setTextSize(1);
  
  display.setCursor(4, 26);
  display.print(F("Az:"));
  display.print(azureResult.substring(0, 10)); // Truncate just in case
  
  display.setCursor(4, 35);
  display.print(F("Mac:"));
  display.print(localResult.substring(0, 9)); 
  
  display.setCursor(4, 44);
  display.print(F("TG:"));
  display.print(telegramResult);
  
  display.setCursor(4, 53);
  display.print(F("Flip switch to reset"));
  
  display.display();
}


void showResetting() {
  display.clearDisplay();
  drawHeader("TRADING KILL SWITCH");
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

void setLED(int r, int g) {
  ledcWrite(PIN_LED_RED,   r);
  ledcWrite(PIN_LED_GREEN, g);
}

void beep(int times, int onMs, int offMs) {
  for (int i = 0; i < times; i++) {
    digitalWrite(PIN_BUZZER, LOW);
    delay(onMs);
    digitalWrite(PIN_BUZZER, HIGH);
    if (i < times - 1) delay(offMs);
  }
}
