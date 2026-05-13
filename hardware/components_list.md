# Components List

Complete hardware shopping list for the ESP32 Trading Kill Switch.

---

## Core Components

| # | Component | Specification | Qty | Est. Price (INR) | Where to Buy |
|---|---|---|---|---|---|
| 1 | ESP32 Dev Board | **38-pin**, ESP-WROOM-32, CP2102, USB-C or Micro USB | 1 | ₹350–450 | Robu.in, Quartz Components |
| 2 | Emergency Stop Button | Mushroom head, 22mm, **latching** type, NO+NC contacts | 1 | ₹150–250 | Amazon India, local electrical shop |
| 3 | OLED Display | 0.96", **I2C**, SSD1306 driver, 128×64 px, 3.3V | 1 | ₹180–250 | Robu.in |
| 4 | RGB LED | 5mm, **common cathode**, clear lens | 1 | ₹10–20 | Any electronics shop |
| 5 | Active Buzzer | **3.3V**, 12mm, with onboard oscillator | 1 | ₹20–30 | Robu.in |
| 6 | Resistor 220Ω | ¼W, 5% tolerance, carbon film | 5 | ₹10–15 (pack of 10) | Any electronics shop |
| 7 | Resistor 10kΩ | ¼W, 5% tolerance, carbon film | 3 | ₹10–15 (pack of 10) | Any electronics shop |
| 8 | ABS Project Enclosure | ~100×68×50mm, with lid | 1 | ₹150–300 | Amazon India |
| 9 | USB 5V 1A Wall Adapter | Micro USB or USB-C (match ESP32 board) | 1 | ₹150–200 | Amazon India |
| 10 | USB Cable | Match your ESP32 board's port | 1 | ₹80–120 | Amazon India |

---

## Prototyping Supplies

| # | Component | Specification | Qty | Est. Price (INR) |
|---|---|---|---|---|
| 11 | Breadboard | 830 tie-point, full size | 1 | ₹80–120 |
| 12 | Jumper wires | Male-to-female, 20cm, pack of 40 | 1 | ₹60–80 |
| 13 | Jumper wires | Male-to-male, 20cm, pack of 40 | 1 | ₹60–80 |

---

## NOT Required

| Component | Why not needed |
|---|---|
| Level shifter (TXS0108, BSS138) | All components run at 3.3V — same as ESP32 logic |
| External antenna | ESP32 onboard PCB antenna is sufficient for indoor WiFi |
| Capacitors | Not required for this application |
| Voltage regulator | ESP32 board has onboard LDO (AMS1117 3.3V) |

---

## Important Notes

### On the emergency stop button — latching vs momentary
**Buy latching type.** A latching button stays physically depressed after you press it (like a pen click) and releases when you twist it. A momentary button only registers while you're pressing it. For an emergency stop, latching is the correct and standard type.

Look for: *"22mm latching mushroom head emergency stop button"*
Avoid: *"momentary push button"*

### On the buzzer — active vs passive
**Buy active buzzer.** An active buzzer has an internal oscillator and makes sound when you simply apply voltage. A passive buzzer needs a PWM signal to generate sound (more complex). Active is simpler and correct for this use.

Look for: *"active buzzer 3.3V"* or check that it says "with oscillator"
Avoid: passive buzzers (they won't beep with a simple GPIO HIGH)

### On the OLED — I2C vs SPI
**Buy I2C version.** I2C uses only 2 wires (SDA + SCL) vs SPI's 4 wires. Much simpler wiring. Almost all 0.96" OLEDs sold in India are I2C — just confirm the listing says "I2C" or "IIC".

---

## Estimated Total Cost

| Category | Cost Range |
|---|---|
| Core components | ₹1,100 – ₹1,500 |
| Prototyping supplies | ₹200 – ₹280 |
| **Total** | **₹1,300 – ₹1,780** |

---

## Recommended Suppliers (India)

| Supplier | Best for | Shipping |
|---|---|---|
| **Robu.in** | ESP32, OLED, sensors, buzzer | Fast, pan-India |
| **Amazon India** | Enclosure, USB adapter, emergency button | Prime delivery |
| **Quartz Components** | ESP32, general electronics | Good stock |
| **SP Road, Bengaluru** | Walk-in, resistors, LEDs, breadboard | Same day |
| **Local electrical shop** | Emergency stop button (22mm panel mount) | Immediate |
