# Smart-Egg-Incubator-Arduino-LabVIEW

An automated egg incubator system that monitors temperature and humidity, controls cooling, and communicates real-time data to a LabVIEW dashboard via serial communication. Built with an Arduino Uno, DHT11 sensor, servo‑driven ventilation window, fan, buzzer, and a potentiometer for manual setpoint adjustment. Optional **IoT upgrade** using the ENC28J60 Ethernet module is also outlined.

![Incubator Simulation Screenshot](path_to_screenshot_image) <!-- replace with actual screenshot if available -->

---

## 📋 Table of Contents
- [Features](#-features)
- [Components & Pinout](#-components--pinout)
- [How It Works](#-how-it-works)
- [Control Logic](#-control-logic)
- [LabVIEW Communication](#-labview-communication)
- [Commands Reference](#-commands-reference)
- [File Structure](#-file-structure)
- [Installation & Setup](#-installation--setup)
- [Future Enhancements](#-future-enhancements)
- [License](#-license)

---

## ✨ Features

- **Temperature & humidity monitoring** with DHT11 sensor.
- **Automatic cooling** when either temperature or humidity exceeds maximum thresholds.
- **Servo‑controlled ventilation window** that opens/closes with the fan.
- **Potentiometer** for on‑the‑fly adjustment of the maximum temperature (25 – 40 °C).
- **Remote control & live data** via LabVIEW (or any serial terminal).
- **Manual fan override** and automatic fallback after 10 seconds of inactivity.
- **Audible alert** (buzzer) when humidity crosses the upper limit.
- **Visual alert** (LED) when temperature is at or above maximum.
- **Simulation support** for Proteus (using COMPIM to interface with LabVIEW).
- **Modular code**, ready for extension with Ethernet / IoT.

---

## 🧱 Components & Pinout

| Component          | Arduino Pin | Notes |
|--------------------|-------------|-------|
| DHT11 sensor       | D7          | Temperature & humidity |
| Potentiometer      | A0          | Sets max temperature (25–40 °C) |
| Overheat LED       | D3          | Lights when temp ≥ max |
| Servo motor        | D4          | Window actuator (0° closed, 90° open) |
| Fan relay / MOSFET | D5          | Cooling fan control (use a relay for high-power fans) |
| Buzzer             | D6          | Sounds when humidity is too high |
| Power              | 5 V & GND    | LiPo battery / USB supply |

> **Note:** The code directly drives the fan from pin D5. If you are using a 12 V fan or a heater, add a relay module or a transistor circuit. The relay mentioned in the project can be used here.

---

## ⚙️ How It Works

1. The system reads temperature and humidity every 100 ms.
2. If the **temperature** reaches or exceeds `maxTemperature`, the LED turns on.
3. If the **humidity** reaches or exceeds `maxHumidity`, the buzzer beeps.
4. When either condition is true, the **fan** and **window** are activated automatically to cool and vent.
5. A **hysteresis** band (3 °C for temp, 5 % for humidity) prevents rapid toggling.
6. Every 2 seconds, the Arduino sends a formatted data packet to LabVIEW:
7. LabVIEW can send back commands to change setpoints or toggle the fan manually.

---

## 🌡️ Control Logic

- **Temperature setpoint** comes either from the potentiometer or from a LabVIEW `T` command.
- After a `T` command is received, it is used for **one** data case, then the system reverts to the potentiometer (unless `AUTO` is sent without arguments).
- The fan and window behave as a pair:
- **When cooling starts**: window opens → delay → fan ON.
- **When cooling stops**: fan OFF → delay → window closes.
- Manual fan control (`F` command) disables automatic cooling until an `AUTO` command or a 10‑second timeout.

---

## 💻 LabVIEW Communication

LabVIEW (or any serial monitor) connects to the Arduino via USB (hardware) or via COMPIM in Proteus simulation.

**Serial parameters:** `9600 baud, 8N1`

### Data sent **to** LabVIEW (every 2 s):
| Field         | Example | Meaning                     |
|---------------|---------|-----------------------------|
| case number   | case20  | Increments by 10 each time  |
| temperature   | 31      | Current temp (°C, integer)  |
| humidity      | 82      | Current humidity (%)        |
| temp_status   | 1       | 1 = temp ≥ max, 0 = normal  |
| fan_status    | 1       | 1 = fan on, 0 = off         |

### Commands **from** LabVIEW:
| Command     | Description                                                | Example   |
|-------------|------------------------------------------------------------|-----------|
| `T 30`      | Set max temperature to 30 °C                               | `T 38`    |
| `H 80`      | Set max humidity to 80 %                                   | `H 65`    |
| `F`         | Toggle fan manually (overrides auto)                       | `F`       |
| `AUTO`      | Return to automatic mode (use potentiometer)               | `AUTO`    |
| `AUTOT 35`  | Return to auto and set temp to 35 °C (used for one cycle)  | `AUTOH 80T 32` |

All commands are **case‑insensitive** and can be combined with `AUTO` (e.g., `AUTOT 35H 70`).

---

## 📂 File Structure
C:.
│ readme.txt # Original brief description
│ future_enhancement.png # Schematic / idea for IoT upgrade
│ New Project [Autosaved].pdsprj # Proteus simulation file
│ Untitled 1.vi # LabVIEW VI (untitled, but working)
│ Enregistrement 2025-12-22 233754.mp4 # Demonstration video
│
├───final_project1 # Arduino sketch folder
│ │ final_project1.ino # Main source code
│ └───build # Compiled hex files
│
├───libraries # Custom libraries for Proteus
│ ├───ENC28J60 Ethernet Module Library
│ └───Fan Models Library
│
└───Project Backups # Older versions of the simulation


---

## 🛠️ Installation & Setup

### Hardware
1. Wire the components as per the pinout table.
2. Power the Arduino (USB or LiPo battery).
3. Connect the USB cable to your PC for LabVIEW or serial monitor.

### Arduino IDE
1. Install the **DHT sensor library** by Adafruit (via Library Manager).
2. Install the **Servo** library (usually built‑in).
3. Open `final_project1/final_project1.ino`.
4. Select **Board: Arduino Uno** and upload.

### LabVIEW (optional)
1. Open the `.vi` file (Untitled 1.vi) – or build your own panel.
2. Configure the VISA serial port (COMx, 9600 baud).
3. Read the incoming string and parse the `caseX: …` format.
4. Send commands via VISA Write when buttons are pressed.

### Proteus Simulation
1. Open the `.pdsprj` file.
2. The COMPIM component handles the virtual serial link to LabVIEW.
3. Run the simulation and test the interaction.

---

## 🔮 Future Enhancements

The `future_enhancement.png` picture outlines an **IoT upgrade** using the **ENC28J60 Ethernet module**. This would allow:

- Remote monitoring via a web server or MQTT.
- Data logging to a cloud platform (ThingSpeak, Blynk, etc.).
- SMS/email alerts when thresholds are exceeded.
- Integration into a smart farm system.

The necessary library (`ENC28J60 Ethernet Module Library`) is already included in the `libraries/` folder for Proteus simulation.

---

