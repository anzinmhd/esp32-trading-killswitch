# Phase 1 Build Log

This document details the development journey, hardware decisions, and troubleshooting steps taken during Phase 1 of the ESP32 Trading Kill Switch project.

## Hardware Selection & Rationale

*   **ESP32 38-pin Dev Board (ESP-WROOM-32, CP2102):** Chosen over standard Arduinos due to its built-in WiFi and Bluetooth capabilities, which will be essential for Phase 2 (connecting to the Upstox API and Azure VPS). It also has plenty of GPIOs for our peripherals.
*   **Lever Switch:** Provides a satisfying, tactile physical interface for an "emergency stop". It prevents accidental bumps better than a standard push-button.
*   **SSD1306 0.96" OLED (I2C):** Perfect for clear, high-contrast feedback regarding the device's state (Armed, Countdown, Killed). I2C minimizes wiring.
*   **5mm Bi-color LED (Red/Green, Common Cathode):** Saves space and GPIO pins compared to using separate LEDs. Allows displaying Green (Armed), Red (Killed), and mixed Yellow (Countdown).
*   **Active Buzzer (Active LOW):** Provides immediate auditory feedback. Chosen active LOW based on the specific module available.

## Prototyping & Simulation

1.  **Wokwi Simulation:** Before ordering or assembling parts, the entire circuit was designed and simulated on [Wokwi](https://wokwi.com/). This allowed for logic testing of the lever switch (simulated as a slide switch), the OLED display, and the LED/Buzzer combination without risking hardware damage.
2.  **Arduino UNO Prototyping:** While waiting for the correct Micro-USB data cable for the ESP32, initial physical prototyping and firmware logic testing were done on an Arduino UNO. This validated the I2C OLED driver and basic switch debouncing logic.

## Transition to ESP32

The move to the ESP32 presented a few environment setup challenges:
*   **The Cable Issue:** Initially, the ESP32 was not detected by the computer. It turned out to be a "charge-only" Micro-USB cable. Replacing it with a proper data cable resolved the hardware connection issue.
*   **CP2102 Drivers on macOS:** The ESP32 board uses the Silicon Labs CP2102 USB-to-UART bridge. Official drivers were downloaded and installed on macOS. The board then correctly appeared at `/dev/cu.SLAB_USBtoUART`.
*   **Flashing:** Flashed via Arduino IDE 2.x, selecting "ESP32 Dev Module" at 115200 baud.

## Wiring & Hardware Design Decisions

*   **Switch Pull-Up:** A 10kΩ pull-up resistor to 3V3 was used on GPIO 4 for the lever switch to ensure a stable HIGH state when the switch is open, preventing floating pin issues.
*   **I2C Pins:** ESP32 requires explicit I2C pin declarations. We routed SDA to GPIO 21 and SCL to GPIO 22.
*   **Active LOW Buzzer:** The buzzer module triggers on a LOW signal. GPIO 18 was assigned to it.
*   **LED Resistors:** 220Ω current-limiting resistors were added to both the Red (GPIO 15) and Green (GPIO 16) legs of the bi-color LED to protect the ESP32 pins and the LED.

## Firmware Decisions

*   **State Machine Architecture:** The firmware is structured around 4 explicit states: `ARMED`, `COUNTDOWN`, `EXECUTING`, and `KILLED`. This ensures clean transitions and prevents overlapping behaviors.
    ```mermaid
    stateDiagram-v2
        [*] --> ARMED : Boot / Reset
        ARMED --> COUNTDOWN : Switch Thrown
        COUNTDOWN --> ARMED : Switch Released (Abort)
        COUNTDOWN --> EXECUTING : Hold > 3 Seconds
        EXECUTING --> KILLED : Kill Sequence Complete
        KILLED --> ARMED : Switch Reset
    ```
*   **3-Second Hold & Debounce:** A 3-second countdown was implemented when the switch is thrown. If the switch is flipped back before 3 seconds, it aborts. A 100ms debounce ensures clean readings from the mechanical lever switch.
*   **ESP32 LEDC PWM (v3.x API):** To achieve the Yellow color for the countdown, both Red and Green need to be mixed. Because ESP32 Arduino core v3.x deprecated `ledcSetup` and `ledcAttachPin`, we used the new `ledcAttach()` API for PWM control.
*   **OLED Configuration:** 
    *   `Wire.setClock(100000)`: Fixed at 100kHz to prevent text corruption observed at higher speeds.
    *   Retries: Added a 5-attempt retry loop for OLED initialization to handle power-on race conditions.

## Troubleshooting & Fixes

*   **OLED Text Ghosting/Overlap:** When updating the countdown numbers, old text remained visible, causing a garbled display. 
    *   *Fix:* Changed text color setting to `display.setTextColor(SSD1306_WHITE, SSD1306_BLACK)` to overwrite the background automatically.
*   **Buzzer Always ON:** The buzzer sounded continuously when the pin was HIGH. 
    *   *Fix:* Inverted the logic in code (HIGH = silent, LOW = beep) as the specific module was Active LOW.
*   **`ledcSetup` Error during Compile:** Compiling failed for PWM functions.
    *   *Fix:* Updated the code to use the new ESP32 Arduino Core v3.x API (`ledcAttach`).

## Current Status (End of Phase 1)

Phase 1 is complete. The physical device is fully assembled. The state machine operates flawlessly, handling the switch, the 3-second abortable countdown, the OLED UI, LED color mixing, and buzzer alerts. The kill sequence currently simulates the shutdown steps visually on the OLED. It is ready for Phase 2 (WiFi and API integration).
