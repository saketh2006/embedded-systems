# STM32-Based Knock Pattern Smart Door Lock
Keyless access using accelerometer-based knock rhythm analysis

---

## Academic Details & Identification

- **Course:** 23CSE304 – Embedded Systems
- **Mentor:** Dr. A. Baskar
- **Department:** Department of Computer Science and Engineering
- **Institution:** School of Computing, Amrita Vishwa Vidyapeetham, Coimbatore, India

### Team Members
| Student Name | Registration Number |
| :--- | :--- |
| **V. Saketh** | CB.SC.U4CSE24558 |
| **Y. Jothi Basu** | CB.SC.U4CSE24559 |
| **T. Rohit Reddy** | CB.SC.U4CSE24551 |
| **T. Kumar** | CB.SC.U4CSE24552 |
| **Vageesh Raj V S** | CB.SC.U4CSE24554 |

---

## 1. Problem Statement
Physical keys, PIN keypads, and access cards can be lost, copied, or stolen, and conventional locks do not alert the owner after a failed attempt. This project provides a keyless alternative: the door unlocks only when the user knocks a secret rhythm on the door surface or wooden pad.

---

## 2. Objectives
- Build a smart door lock system on the **STM32F401CCU6 (Black Pill)** microcontroller.
- Detect knock vibrations using an **ADXL345 3-axis accelerometer** over I²C.
- Recognize a secret rhythm from the knock count and the time intervals between knocks.
- Control a door latch using an **SG90 servo motor** driven by hardware PWM.
- Display status messages on a **16×2 I²C LCD**.
- Send status and rejection alerts to a phone over **HC-05 Bluetooth (USART)**.
- Calibrate knock sensitivity using a 10 kΩ potentiometer read via ADC.
- Provide a push button for manual mode toggle.

---

## 3. Proposed Solution
An STM32F401CCU6 microcontroller samples the ADXL345 accelerometer every 20 ms. When an impact exceeds the threshold set by the potentiometer, the firmware records the knock timestamp. A valid pattern consists of three rhythmic knocks where intervals fall between 150 ms and 1000 ms, with an interval timing difference of $\le 35\%$. A correct pattern rotates the servo to unlock position, while an incorrect pattern denies access.

---

## 4. System Workflow & Working

### Six-Step Workflow:
1. **Step 1 — Power On & Initialization:** Firmware initializes SysTick timer (1 ms timebase), GPIOs, I²C1 bus, 12-bit ADC1, USART2 serial engine (9600 baud), and TIM3 PWM channel.
2. **Step 2 — Motion & Vibration Sensing:** ADXL345 digital accelerometer transmits 3-axis acceleration data $(x, y, z)$ over I²C every 20 ms.
3. **Step 3 — Sensitivity Thresholding:** 12-bit ADC samples the 10 kΩ potentiometer wiper (PA0) to adaptively adjust the vibration threshold against varying door densities.
4. **Step 4 — Temporal Rhythm Recording:** SysTick captures precise timestamps on qualifying impact spikes; inter-knock intervals ($T_1, T_2$) are recorded and debounced (180 ms lockout).
5. **Step 5 — Pattern Verification & Decision:** Firmware evaluates total knock count, interval boundaries (150 ms to 1000 ms), and relative cadence symmetry ($|T_1 - T_2| \le 35\% \times T_1$).
6. **Step 6 — Actuation & State Reset:**
   - **Pattern Match (Access Granted):** LCD shows `ACCESS GRANTED` / `DOOR UNLOCKED`, SG90 servo rotates to unlock position (1000 µs), HC-05 transmits unlock notification to phone. After 5 seconds (5000 ms), the latch automatically relocks.
   - **Pattern Mismatch / Timeout (Access Denied):** LCD shows `ACCESS DENIED` / `DOOR LOCKED`, servo remains locked (2000 µs), HC-05 transmits rejection alert, and system resets to listening mode.
   - **Manual Bluetooth Override:** When knock mode is toggled off via push button (PA1), entering the 4-digit PIN (`9999`) via Bluetooth unlocks the door directly.

### Workflow Diagram:
```mermaid
flowchart TD
    A([Power On]) --> B[Initialize SysTick, GPIO, I2C1, ADC1, USART2, TIM3]
    B --> C[Sample Potentiometer via ADC1 PA0 -> Compute Threshold]
    C --> D[Sample ADXL345 Acceleration over I2C1]
    D --> E{Impact > Threshold?}
    E -- No --> D
    E -- Yes --> F[Record Knock Timestamps & Measure Intervals T1, T2]
    F --> G{3 Knocks & Intervals in 150-1000ms?}
    G -- No / Timeout --> H[Access Denied: LCD 'ACCESS DENIED', Door Stays Locked, BT Alert]
    G -- Yes --> I{Timing Difference <= 35%?}
    I -- No --> H
    I -- Yes --> J[Access Granted: LCD 'ACCESS GRANTED', SG90 Servo Unlocks, BT Alert]
    J --> K[Wait 5000ms Auto-Relock Timeout]
    K --> L[Servo Relocks to 2000us]
    H --> D
    L --> D
```

---

## 5. Hardware Pin Mapping

| Pin | Function | Peripheral | Connects To |
| :--- | :--- | :--- | :--- |
| **PB8** | I2C1_SCL | I²C1 | ADXL345 SCL and 16×2 LCD SCL (shared bus) |
| **PB9** | I2C1_SDA | I²C1 | ADXL345 SDA and 16×2 LCD SDA (shared bus) |
| **PA0** | ADC1_IN0 | ADC1 | 10 kΩ Potentiometer wiper (knock sensitivity) |
| **PA1** | GPIO Input | GPIO | Push Button to GND (active-low, internal pull-up) |
| **PA2** | USART2_TX | USART2 | HC-05 Bluetooth RXD |
| **PA3** | USART2_RX | USART2 | HC-05 Bluetooth TXD |
| **PA6** | TIM3_CH1 | Timer 3 PWM | SG90 Servo signal pin |
| **3.3V** | Power | STM32 3V3 | ADXL345 and LCD logic |
| **5V (Ext)**| Power | Regulated 5V 2A | SG90 Servo VCC (isolated from MCU rail) |
| **GND** | Ground | Ground Plane | Common GND for all modules and power supply |

---

## 6. Project Files and Documentation

All circuit schematics, prototype photos, presentations, and documents are organized inside the repository:

- **Circuit Diagram:** `docs/images/circuit_diagram.png`
- **Prototype Photos:** `docs/images/prototype_wooden_model.png`, `docs/images/prototype_exterior.jpg`, `docs/images/prototype_interior.jpg`
- **Architecture Diagram:** `docs/images/prototype_overview_diagram.png`
- **Case Study Report:** `docs/STM32_Knock_Pattern_Case_Study_Report.docx`
- **Presentation Deck:** `docs/GROUP_9_Knock_Pattern_Smart_Door_Lock.pptx`
- **Firmware Source:** `main.c` and `src/main.c`
- **Web App Dashboard:** `web_app/` (`index.html`, `server.py`, `run_app.bat`)

---

## 7. Web Application (HC-05 Telemetry Dashboard)
An interactive web dashboard connects to the HC-05 Bluetooth module over Web Serial (9600 baud):
- Real-time knock progress visualization
- Live potentiometer sensitivity gauge and waveform chart
- Remote lock/unlock PIN interface (`9999`)
- Bidirectional serial log terminal

To launch:
```bash
python server.py
# Or run run_app.bat
```
Open `http://127.0.0.1:8085` in Google Chrome or Microsoft Edge.

---

## 8. How to Run the Code (Step-by-Step Guide)

### Part 1: Hardware Connections
1. Connect the **ST-LINK V2** programmer to the STM32F401CCU6 Black Pill:
   - `SWDIO` &rarr; `SWDIO` (PA13)
   - `SWCLK` &rarr; `SWCLK` (PA14)
   - `GND` &rarr; `GND`
   - `3.3V` &rarr; `3.3V`
2. Wire all peripheral components as per the **Hardware Pin Mapping** table:
   - **ADXL345 & 16×2 LCD:** SCL to `PB8`, SDA to `PB9`, VCC to `3.3V`, GND to `GND`.
   - **Potentiometer:** Wiper to `PA0`, outer legs to `3.3V` and `GND`.
   - **Push Button:** One pin to `PA1`, the other pin to `GND`.
   - **HC-05 Bluetooth:** TXD to `PA3`, RXD to `PA2`, VCC to `5V`/`3.3V`, GND to `GND`.
   - **SG90 Servo:** Signal wire to `PA6`. **Connect Red (VCC) and Brown/Black (GND) to an external regulated 5V 2A power supply, with a common ground joined to the STM32 GND.**

---

### Part 2: Compiling & Flashing the Firmware

#### Option A: Using Keil MDK-ARM (Recommended)
1. Open Keil uVision and create a new project targeting device **STM32F401CCUx**.
2. In the **Manage Run-Time Environment** dialog, check:
   - `CMSIS` &rarr; `CORE`
   - `Device` &rarr; `Startup`
3. Add `main.c` (or `src/main.c`) into your project source group.
4. Open **Project Options (Alt + F7)**:
   - **Target Tab:** Set IROM1 to `0x08000000` (Size: `0x40000`), IRAM1 to `0x20000000` (Size: `0x10000`).
   - **Debug Tab:** Choose **ST-Link Debugger**, open *Settings*, and confirm SWD detects the Cortex-M4 core.
   - **Utilities Tab:** Enable *Use Target Driver for Flash Programming*.
5. Click **Build (F7)** to compile the code (verifying 0 Errors, 0 Warnings).
6. Click **Download (F8)** to flash the binary to the board.
7. Press the **NRST** hardware button on the Black Pill board to reset and start firmware execution.

#### Option B: Using STM32CubeIDE
1. Create a new **STM32 Project** selecting MCU **STM32F401CCU6**.
2. Copy `main.c` into the `Core/Src/` folder.
3. Build the project (**Project &rarr; Build Project**).
4. Connect ST-LINK V2 and click **Run &rarr; Run As &rarr; STM32 Cortex-M C/C++ Application**.

---

### Part 3: Running the Web App & Bluetooth Control Center
1. **Pair HC-05 with Computer:**
   - In Windows, go to **Settings &rarr; Bluetooth & devices &rarr; Add device**.
   - Select **HC-05** and pair using PIN `1234`.
2. **Launch Local Web Server:**
   - Double-click `run_app.bat` in the repository root (or run `python server.py`).
   - The browser opens automatically to `http://127.0.0.1:8085`.
3. **Connect to Telemetry:**
   - Verify the baud rate selector is set to **9600**.
   - Click **"Connect HC-05"** and select the paired Bluetooth Serial COM port.
4. **Operation & Verification:**
   - **Knock Mode:** Knock a 3-beat rhythm on the door. The LCD displays `Knock: [*  ]` &rarr; `[** ]` &rarr; `[***] OK`. The servo unlocks and relocks after 5 seconds.
   - **Dynamic Calibration:** Rotate the potentiometer; verify the threshold gauge updates live on the web interface.
   - **PIN Mode Override:** Press the PA1 push button to toggle off knock mode, then send `9999` from the keypad to unlock.

---

## 9. Testing Plan
- **Correct Pattern:** 20 consecutive attempts using the stored rhythm; door should unlock on each attempt.
- **Wrong Pattern:** 20 attempts with irregular rhythms; door must remain locked.
- **Sensitivity:** Test soft, medium, and hard knocks across different potentiometer settings.
- **Timing Tolerance:** Verify gap tolerance ($\le 35\%$).
- **Bluetooth Range:** Test reception at 1 m, 5 m, and 10 m.
- **Servo Reliability:** 20 lock/unlock cycles to ensure latch clearance without power sag.

---

## 10. Limitations & Future Work
- **Limitations:** Knock rhythms can be overheard; secret pattern is stored in firmware; HC-05 uses unencrypted SPP.
- **Future Work:** Allow user-programmable rhythm storage in flash memory, add lockout after repeated failed attempts, upgrade to encrypted BLE, and use ADXL345 interrupt lines for low-power sleep modes.

---

## 11. Acknowledgement
We express our sincere gratitude to our mentor, **Dr. A. Baskar**, for his valuable academic guidance and support throughout this case study carried out under the course **23CSE304 – Embedded Systems**, Department of Computer Science and Engineering, School of Computing, Amrita Vishwa Vidyapeetham, Coimbatore, India.
