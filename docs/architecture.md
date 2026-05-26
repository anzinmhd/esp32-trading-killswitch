# Architecture

Detailed design decisions and system architecture for the ESP32 Trading Kill Switch.

---

## Design Philosophy

### Hardware-first safety
The kill switch is intentionally a **separate physical device** from the trading bot. This means:

- If the trading bot process crashes or freezes → kill switch still works
- If the laptop running the bot loses power → kill switch (on USB adapter) still works
- If the bot's network stack has issues → kill switch uses its own WiFi connection
- No shared process, no shared memory, no shared network socket

This is the key difference from a software kill switch. The hardware layer is the last line of defense.

### 3-second hold requirement
A single button press does NOT trigger the kill sequence. The button must be held for **3 continuous seconds**. This prevents:
- Accidental brushing of the button
- Bumping the device
- False triggers from electrical noise

The OLED counts down during the hold so you know it's registering.

---

## Component Architecture

```mermaid
flowchart LR
    subgraph ESP32["ESP32 (38-pin)"]
        direction TB
        
        subgraph GPIO["GPIO Layer"]
            G4[GPIO 4]
            G18[GPIO 18]
            G15[GPIO 15]
            G16[GPIO 16]
            G21[GPIO 21]
            G22[GPIO 22]
        end
        
        subgraph Logic["Application Logic"]
            BtnISR[Button ISR + debounce]
            BuzzCtrl[Buzzer control]
            LEDR[LED Red]
            LEDG[LED Green]
            OLED_SDA[I2C SDA OLED]
            OLED_SCL[I2C SCL OLED]
        end
        
        subgraph Mem["Flash Memory"]
            Sec[secrets.h tokens]
        end
        
        subgraph WiFi["WiFi Stack (built-in)"]
            WPA[WPA2, 2.4GHz]
            TLS[TLS 1.2 for HTTPS]
        end

        G4 --> BtnISR
        Logic -->|Buzzer| G18
        Logic -->|Red| G15
        Logic -->|Green| G16
        Logic -->|SDA| G21
        Logic -->|SCL| G22
        
        Sec --> Logic
        Logic --> WiFi
    end

    WiFi -->|HTTPS TLS| Upstox[Upstox API v2]
    WiFi -->|HTTPS TLS| Telegram[Telegram Bot API]
```

---

## Kill Sequence - Detailed Flow

```mermaid
flowchart TD
    A[Button held > 3 sec] --> B[Set LED YELLOW]
    B --> C[OLED: '...']
    C --> D[GET /v2/order/ <br> fetch pending orders]
    D --> E[DELETE each order via /v2/orders/id]
    E --> F[GET /v2/portfolio/positions]
    F --> G[POST /v2/order/place <br> opposing market order]
    G --> H[POST Telegram alert with summary]
    H --> I[LED -> RED]
    I --> J[Buzzer x3]
    J --> K[OLED: 'DONE']
```

---

## LED Status Codes

| Color | Meaning |
|---|---|
| 🟢 Green | Armed, WiFi connected, ready |
| 🟡 Yellow (blinking) | Connecting to WiFi on boot |
| 🟡 Yellow (solid) | Kill sequence executing |
| 🔴 Red | Kill executed successfully |
| 🔴 Red (fast blink) | Error - API call failed |
| Off | No power |

---

## Upstox API Endpoints Used

| Action | Method | Endpoint |
|---|---|---|
| Get open positions | GET | `/v2/portfolio/short-term-positions` |
| Get pending orders | GET | `/v2/orders` |
| Cancel a specific order | DELETE | `/v2/orders/{order_id}` |
| Place closing order | POST | `/v2/order/place` |

All calls use:
- Base URL: `https://api.upstox.com`
- Header: `Authorization: Bearer {access_token}`
- Header: `Content-Type: application/json`
- TLS via `WiFiClientSecure`

---

## Bot Kill Signal

The kill switch signals your trading bot to stop placing new orders via one of two methods (choose based on your bot's architecture):

### Option A - Shared file flag (simplest)
The ESP32 calls a lightweight webhook on your local machine or server that writes a file:
```
/tmp/TRADING_KILL_ACTIVE
```
Your bot checks for this file at the start of each trade cycle and exits if it exists.

### Option B - HTTP webhook
Your trading bot exposes a local endpoint:
```
POST http://your-bot-server:8080/kill
```
The ESP32 calls this endpoint. Your bot shuts down its order loop on receipt.

### Option C - MQTT (advanced)
Both the ESP32 and bot subscribe to an MQTT broker. ESP32 publishes `kill` to a topic; bot subscribes and halts.

**Recommended for paper trading phase:** Option A (simplest, no extra infra).

---

## Security Considerations

### What's in secrets.h (never commit)
```
WIFI_SSID
WIFI_PASSWORD
UPSTOX_ACCESS_TOKEN
TELEGRAM_BOT_TOKEN
TELEGRAM_CHAT_ID
```

### Token storage
Tokens are stored in ESP32 flash memory via `secrets.h` compiled into firmware. For production, consider using ESP32's NVS (Non-Volatile Storage) with encryption.

### Physical security
The device should be:
- Within arm's reach of the trader at all times
- In a clearly labelled enclosure (RED label: "EMERGENCY STOP")
- Never left unattended in a public space (WiFi credentials are in flash)

---

## Future Improvements

- [ ] Auto-refresh Upstox access token (OAuth refresh flow)
- [ ] Battery backup (Li-Po + TP4056 module) so it works during power cuts
- [ ] Second confirmation button (two-button press = kill) for extra safety
- [ ] SD card logging of all kill events with timestamps
- [ ] Web dashboard (ESP32 hosts a tiny web server) to see status
- [ ] Support for multiple brokers (Zerodha, Angel One)
- [ ] OTA (Over-the-Air) firmware updates
