# Phase 2 Implementation Plan

This document outlines the roadmap for Phase 2 of the ESP32 Trading Kill Switch project. The goal of Phase 2 is to move from local simulation to full network integration, allowing the hardware switch to actually halt the Trade-Lab AI bot and close market positions.

## 1. WiFi Connection Strategy

*   **Library:** Use the standard `<WiFi.h>` library for ESP32.
*   **Connection Flow:**
    *   On boot, the OLED will display "CONNECTING WIFI...".
    *   It will attempt to connect to the predefined SSID.
    *   If successful, OLED displays "WIFI OK" followed by the local IP, then transitions to the `ARMED` state.
    *   If it fails after a timeout (e.g., 10 seconds), it will display "WIFI FAILED" and halt, requiring a reset.
*   **Security:** WiFi credentials will be stored in `secrets.h` (which is in `.gitignore`) to prevent accidental commits to the repository.

## 2. Upstox API Integration

The core function of the kill switch is to flatten the portfolio immediately. The ESP32 will make secure HTTPS requests directly to the Upstox API.

*   **Library:** `<HTTPClient.h>` and `<WiFiClientSecure.h>`.
*   **Authentication:** The ESP32 will use the same read/write API token used by the Trade-Lab bot (stored in `secrets.h`).
*   **API Calls Needed:**
    ```mermaid
    sequenceDiagram
        participant E as ESP32
        participant U as Upstox API
        
        E->>U: GET /v2/order/retrieve-all
        U-->>E: List of pending orders
        loop For each order
            E->>U: DELETE /v2/orders/{id}
            U-->>E: 200 OK
        end
        
        E->>U: GET /v2/portfolio/short-term-positions
        U-->>E: List of open positions
        loop For each position
            E->>U: POST /v2/order/place (Market Sell)
            U-->>E: 200 OK
        end
    ```
    1.  **Cancel All Open Orders:**
        *   **Endpoint:** `DELETE https://api.upstox.com/v2/order/multi/cancel` (or iterating through open orders via `GET /v2/order/retrieve-all`).
        *   **Action:** Ensure no pending buys or sells execute.
    2.  **Close All Positions:**
        *   **Endpoint:** `GET https://api.upstox.com/v2/portfolio/short-term-positions` to retrieve open positions.
        *   **Action:** Iterate through open positions and send market sell orders for each holding using `POST https://api.upstox.com/v2/order/place`.

*Note: Since Trade-Lab is currently in the paper trading phase, these API calls will initially be directed to the Upstox Sandbox/Paper environment or simulated via the Trade-Lab webhook until live trading commences.*

## 3. Azure VPS Webhook Design

If the Upstox API is inaccessible from the ESP32, or to ensure the Python bot stops scanning and making decisions, the ESP32 will signal the Azure VM directly.

*   **Webhook Endpoint:** Add a lightweight Flask server to the Trade-Lab `main.py` orchestrator, listening on a specific port (e.g., 5000).
    *   `POST /api/kill`
*   **Authentication:** A simple pre-shared key (PSK) in the header (e.g., `X-Kill-Auth: <secret_token>`).
*   **Bot Action:** When the webhook is received:
    ```mermaid
    sequenceDiagram
        participant E as ESP32
        participant F as Flask Webhook
        participant M as main.py (Trade-Lab)
        
        E->>F: POST /api/kill (Header: X-Kill-Auth)
        F->>F: Create .kill_flag file
        F-->>E: 200 OK (Kill signal accepted)
        
        loop Every loop iteration
            M->>M: Check for .kill_flag
            opt flag exists
                M->>M: Abort active scan
                M->>M: Cancel pending AI calls
                M->>M: Liquidate paper broker
                M->>M: Exit Process
            end
        end
    ```
    1.  The Flask server creates a local `.kill_flag` file in the Trade-Lab directory.
    2.  The main loop of Trade-Lab checks for this file. If found, it instantly aborts the scan, cancels all pending AI API calls, and shuts down the process.

## 4. Telegram Alert Design

To notify the user that the kill switch was activated, the ESP32 will send a Telegram message.

*   **Endpoint:** `POST https://api.telegram.org/bot<TOKEN>/sendMessage`
*   **Payload:**
    ```json
    {
      "chat_id": "<CHAT_ID>",
      "text": "🚨 **EMERGENCY KILL SWITCH ACTIVATED** 🚨\n\nHardware switch was thrown.\n- Bot process halted\n- Open orders cancelled\n- Positions flattened\n\nManual intervention required.",
      "parse_mode": "Markdown"
    }
    ```

## 5. Integration with Paper Trading Phase

Because Trade-Lab is currently managing a ₹1,00,000 simulated portfolio via `paper_broker.py`:
*   The ESP32 will primarily rely on the **Azure VPS Webhook** for the kill action during paper trading. 
*   The webhook will tell `paper_broker.py` to liquidate all paper positions at the current market price and write the final P&L to the trade journal before exiting.
*   Once Trade-Lab moves to the Live Phase with real capital, the direct Upstox API calls will be enabled on the ESP32 for true redundancy.

## 6. Testing Plan

Before declaring Phase 2 live:
1.  **WiFi Robustness:** Test device behavior when the router is restarted or the connection drops. It should attempt reconnection.
2.  **Webhook Test:** Trigger the switch while Trade-Lab is actively doing a morning scan (paper trading) and verify it aborts cleanly without data corruption in the journal.
3.  **API Mocking:** Use a service like Postman or a local mock server to verify the HTTP payloads for Upstox and Telegram are formatted correctly before sending them over the internet.
