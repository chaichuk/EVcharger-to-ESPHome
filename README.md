# ChargeU EV Charger Gateway (ESPHome)
![ЧИТАТИ УКРАЇНСЬКОЮ]()

An advanced ESPHome-based gateway to integrate **ChargeU Base** EV chargers into **Home Assistant**.

This project uses a **WT32-ETH01** board to act as a smart bridge: it maintains a stable connection to Home Assistant via **Ethernet** while simultaneously connecting to the ChargeU charger's Access Point via **WiFi**.

![ChargeU HA Card](images/card_example.png)


## 🚀 Features

* **Dual Connectivity:** Leverages Ethernet for rock-solid HA communication and WiFi for the Charger control.
* **Full Control via Home Assistant:**
    * **Amperage Control:** Set charging limit from 6A to 32A.
    * **LED Control:** Toggle the charger's LED indication.
    * **Ground Check:** Toggle the grounding verification logic.
    * **Station Lock:** Lock/Unlock the station (passwordless protection mechanism).
    * **One-Time Session:** Enable/Disable single charging sessions.
* **Smart Polling & Queuing:**
    * Implements a "Smart Queue" to prevent overloading the charger's limited web server capabilities.
    * **Polling Interval:** 6 seconds for live data, on-demand updates for settings.
* **Optimistic UI:** Provides instant feedback in the Home Assistant dashboard with background verification (eliminates UI lag).
* **Robust Connectivity:** Includes "Anti-Flap" logic to filter out short WiFi dropouts, ensuring sensor status remains stable even if the WiFi signal fluctuates.
* **Comprehensive Sensors:**
    * Voltage (V)
    * Current (A)
    * Session Energy (kWh)
    * Total Energy (kWh)
    * Session Duration (Formatted HH:MM:SS)
    * Connection Status

## 🛠 Hardware Required

* **WT32-ETH01** (ESP32 board with built-in LAN8720 Ethernet).
* **Power Supply:** 5V/1A minimum (connected via 5V/GND pins).
    * *Note:* Do not power the board via a USB-TTL adapter during operation; high current spikes from the WiFi module may cause instability.
* Ethernet cable.

## ⚙️ Installation

### 1. Prepare the Component File
Create a file named `chargeu_component.h` in your ESPHome configuration directory (e.g., `/config/esphome/`) and paste the C++ logic code provided in this repository.

**Important:** Edit the top lines of `chargeu_component.h` to match your charger's credentials:
```cpp
const char* CHARGER_SSID = "CHARGEU base xxxx"; 
const char* CHARGER_PASS = "your_password";
const char* CHARGER_URL  = "[http://192.168.4.1](http://192.168.4.1)";
```

### 2. Create ESPHome Configuration
Create a new ESPHome configuration file (e.g., `chargeu-gateway.yaml`) and copy the YAML configuration provided in this repository.

> **Configuration Note:** This project uses `board: esp32dev` generic profile instead of `wt32-eth01` specific profile to strictly manage the Ethernet power pins and avoid driver conflicts.

### 3. Flash the Board
1. Connect the WT32-ETH01 to your computer via a USB-TTL adapter (remember to bridge **IO0** to **GND** for flashing mode).
2. Flash the firmware using the ESPHome Dashboard.
3. Disconnect USB, remove the IO0-GND bridge.
4. Connect the board to the 5V Power Supply and Ethernet cable.
