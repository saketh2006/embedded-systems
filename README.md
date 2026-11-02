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

## 4. System Working
1. **Power-On:** Hardware peripherals (SysTick, GPIO, I²C1, ADC1, USART2, TIM3 PWM) are initialized.
2. **Knock Detection:** ADXL345 detects vibration spikes. The potentiometer sets the sensitivity threshold.
3. **Pattern Validation:** The firmware measures inter-knock intervals $T_1$ and $T_2$.
   - **Correct Pattern:** LCD shows `ACCESS GRANTED` / `DOOR UNLOCKED`, the servo moves to the open position (1000 µs), and an unlock notification is sent via Bluetooth. The door automatically relocks after 5 seconds.
   - **Incorrect Pattern:** LCD shows `ACCESS DENIED` / `DOOR LOCKED`, the servo stays locked (2000 µs), and a rejection alert is sent via Bluetooth.
4. **Bluetooth Override:** If knock mode is disabled via the push button, the door can also be unlocked by sending the PIN (`9999`) via Bluetooth.

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

## 8. Testing Plan
- **Correct Pattern:** 20 consecutive attempts using the stored rhythm; door should unlock on each attempt.
- **Wrong Pattern:** 20 attempts with irregular rhythms; door must remain locked.
- **Sensitivity:** Test soft, medium, and hard knocks across different potentiometer settings.
- **Timing Tolerance:** Verify gap tolerance ($\le 35\%$).
- **Bluetooth Range:** Test reception at 1 m, 5 m, and 10 m.
- **Servo Reliability:** 20 lock/unlock cycles to ensure latch clearance without power sag.

---

## 9. Limitations & Future Work
- **Limitations:** Knock rhythms can be overheard; secret pattern is stored in firmware; HC-05 uses unencrypted SPP.
- **Future Work:** Allow user-programmable rhythm storage in flash memory, add lockout after repeated failed attempts, upgrade to encrypted BLE, and use ADXL345 interrupt lines for low-power sleep modes.

---

## 10. Acknowledgement
We express our sincere gratitude to our mentor, **Dr. A. Baskar**, for his valuable academic guidance and support throughout this case study carried out under the course **23CSE304 – Embedded Systems**, Department of Computer Science and Engineering, School of Computing, Amrita Vishwa Vidyapeetham, Coimbatore, India.
