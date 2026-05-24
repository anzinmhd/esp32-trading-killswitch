# Hardware Components List

Below is the definitive list of hardware components used to build the Trade-Lab ESP32 Kill Switch (Phase 1).

## Core Components

| Component | Description / Specification | Quantity | Notes |
| :--- | :--- | :--- | :--- |
| **Microcontroller** | ESP32 38-pin Dev Board (ESP-WROOM-32) | 1 | Must have CP2102 USB-to-UART chip. Chosen for built-in WiFi/Bluetooth required for Phase 2. |
| **Display** | SSD1306 0.96" OLED | 1 | I2C interface (4 pins: VCC, GND, SCL, SDA). 128x64 resolution. |
| **Switch** | Lever Switch | 1 | 2-pin (centre + left/right). Provides tactile feedback for emergency activation. |
| **Indicator LED** | 5mm Bi-color LED (Red/Green) | 1 | Common Cathode (3-pin). Allows displaying Armed (Green), Countdown (Yellow - mixed), and Killed (Red). |
| **Auditory Alarm** | Active Buzzer Module | 1 | 3-pin (VCC, I/O, GND). **Crucial note:** The module used is Active LOW (LOW = beep, HIGH = silent). |

## Supporting Electronics

| Component | Description / Specification | Quantity | Notes |
| :--- | :--- | :--- | :--- |
| **Resistors** | 220Ω | 2 | Current-limiting resistors for the Red and Green LED legs to protect the ESP32 GPIOs. |
| **Resistor** | 10kΩ | 1 | Pull-up resistor for the lever switch to ensure a stable HIGH reading when open. |
| **Prototyping** | Breadboard & Jumper Wires | 1 set | For assembling the circuit without soldering during Phase 1. |
| **Cable** | Micro-USB Data Cable | 1 | **Must support data transfer.** See "Lessons Learned" below. |

---

## Lessons Learned & Purchase Warnings

If you are sourcing parts for a similar build, keep the following in mind based on our Phase 1 troubleshooting:

### ⚠️ What NOT to buy (or use)

1.  **Charge-only Micro-USB Cables:** Do not use cheap USB cables often bundled with power banks or small devices. They lack the D+ and D- data lines. Your computer will not detect the ESP32 (no `/dev/cu.*` port will appear), making it impossible to flash firmware. **Ensure the cable is explicitly a Data/Sync cable.**
2.  **I2C Logic Level Shifters:** Some older guides suggest using level shifters for I2C devices on 3.3V boards. The SSD1306 used in this project operates perfectly at 3.3V logic provided by the ESP32. A level shifter is unnecessary and adds point-of-failure wiring complexity.

### 💡 Component Specifics

*   **Active Buzzer (Active LOW logic):** Pay close attention to your buzzer module. While many tutorials assume HIGH = ON, the specific module used here triggers on a LOW signal. We had to invert the logic in our C++ firmware to prevent the buzzer from staying on permanently.
*   **Bi-color LED Pinout:** Before wiring, test the LED with a 3V coin cell battery (using a resistor). Identifying the common cathode (usually the longest leg) and which of the remaining legs corresponds to Red vs. Green is critical for getting the "Armed" and "Killed" colors correct on the first try.
*   **ESP32 38-pin vs 30-pin:** This project uses a 38-pin board. Ensure your breadboard layout matches the pinout diagram for your specific board variant, as 30-pin ESP32s have different pin locations for GPIOs like 15 and 16.
