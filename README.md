# Moving Speaker (Arduino)

This repository contains the Arduino firmware for the "Moving Speaker" project (control of two stepper motors to aim a speaker). The main firmware is in `src/main.cpp`.

**Hardware**
- The firmware is developed and tested for the Seeed Studio XIAO ESP32C3. 

**Purpose**
- Drive four stepper motors organized as two couples: motors A/C and motors B/D, with control over position, speed and acceleration.
- Communicate with a PC interface over a serial link (115200 baud) to receive setpoints and return status.

**Demo**
You can download demo/demo.mp4 video file


**Key firmware files**
- Main application: `src/main.cpp`
- Stepper control: `src/stepper.h`, `src/stepper.cpp`

---
**Serial communication settings**
- Baud rate: `115200`
- Format: ASCII lines terminated by newline (`\n`).
- The firmware periodically emits a status frame and responds to commands from the PC.

---
**Frames sent by the Arduino**

1) Startup information frames
- The Arduino prints some information at startup:
	- `I: Moving Speaker V1.0 by Détourner`
	- `I:` followed by a line with 24 comma-separated values (no extra prefix) that describe the limits and ranges for motors A and B. Motors C and D use the same range definitions as A and B respectively.

	Order of the 24 values (comma-separated):
	- (1) motor A, min position in degree 
	- (2) motor A, max position in degree
	- (3) motor A, min speed in °/s
	- (4) motor A, max speed in °/s
	- (5) motor A, min acceleration in °/s^2
	- (6) motor A, max acceleration in °/s^2
	- (7) motor B, min position in degree
	- (8) motor B, max position in degree
	- (9) motor B, min speed in °/s
	- (10) motor B, max speed in °/s
	- (11) motor B, min acceleration in °/s^2
	- (12) motor B, max acceleration in °/s^2
	- same for motor C and D

	Example (console):
	I:
	-90,90,0.1,20,1,100,0,359.99,0.1,20,1,100,-90,90,0.1,20,1,100,0,359.99,0.1,20,1,100

2) Periodic status frames (`P:`)
- Emitted approximately every 100 ms (controlled in the main loop).
- Format:
	P:isRunningA,positionA_deg,speedA_degPerSec,isRunningB,positionB_deg_modulo,speedB_degPerSec,isRunningC,positionC_deg,speedC_degPerSec,isRunningD,positionD_deg,speedD_degPerSec

	Field details:
	- `isRunningA`: `0` or `1` (motor A is moving)
	- `positionA_deg`: current position in degrees (may include two decimals)
	- `speedA_degPerSec`: current speed in degrees/s
	- `isRunningB`: `0` or `1` (motor B is moving)
	- `positionB_deg_modulo`: B's position normalized modulo 360° (0..360)
	- `speedB_degPerSec`: current speed in degrees/s
	- `isRunningC`: `0` or `1` (motor C is moving)
	- `positionC_deg`: current position in degrees (same semantics as A)
	- `speedC_degPerSec`: current speed in degrees/s
	- `isRunningD`: `0` or `1` (motor D is moving)
	- `positionD_deg_modulo`: D's position normalized modulo 360° (same semantics as B)
	- `speedD_degPerSec`: current speed in degrees/s

	Example:
	P:1,12.34,5.00,0,270.00,0.00,1,12.34,5.00,0,90.00,0.00

3) Confirmation frames after a command is received (`S:`)
- When the Arduino receives and accepts a command frame, it replies with:
	S:isRunningA,targetA_deg,maxSpeedA_deg,accelA_degPerSec,isRunningB,targetB_deg,maxSpeedB_deg,accelB_degPerSec,isRunningC,targetC_deg,maxSpeedC_deg,accelC_degPerSec,isRunningD,targetD_deg,maxSpeedD_deg,accelD_degPerSec

	Example:
	S:1,45.00,17.00,50.00,0,90.00,17.00,50.00,1,45.00,17.00,50.00,0,90.00,17.00,50.00

4) Error frames (`E:`)
- Format error (wrong number of fields):
	E:Invalid frame: wrong number of fields

---
**Command format (PC → Arduino)**

The firmware expects a single CSV line (no prefix) containing exactly 14 fields separated by commas, followed by a newline (`\n`).

Order of the 14 fields:
1. `motA_target`  — target position for motor A (degrees)
2. `motA_speed`   — maximum speed for A (degrees/s)
3. `motA_accel`   — acceleration for A (degrees/s²)
4. `motB_target`  — target position for motor B (degrees). Position for motor B is handled modulo a revolution.
5. `motB_speed`   — maximum speed for B (degrees/s)
6. `motB_dir`     — rotation mode for B (integer):
	 - `0` = ROT_SHORTEST (shortest path)
	 - `1` = ROT_CW (clockwise only)
	 - `2` = ROT_CCW (counter-clockwise only)
7. `motB_accel`   — acceleration for B (degrees/s²)
8. `motC_target`  — target position for motor C (degrees)
9. `motC_speed`   — maximum speed for C (degrees/s)
10. `motC_accel`   — acceleration for C (degrees/s²)
11. `motD_target`  — target position for motor D (degrees). Position for motor D is handled modulo a revolution.
12. `motD_speed`   — maximum speed for D (degrees/s)
13. `motD_dir`     — rotation mode for D (integer):
	 - `0` = ROT_SHORTEST (shortest path)
	 - `1` = ROT_CW (clockwise only)
	 - `2` = ROT_CCW (counter-clockwise only)
14. `motD_accel`   — acceleration for D (degrees/s²)

Notes:
- All fields are ASCII decimal numbers; floating point values are accepted where relevant.
- The line must contain exactly 13 commas (14 fields). Otherwise the Arduino will return an `E:` error frame.

Command example (terminated by `\n`):
```
10.0,150.0,200.0,180.0,120.0,0,300.0,10.0,150.0,200.0,180.0,120.0,0,300.0
```
This means:
- Motor A target → 10.0° with vmax 150°/s and accel 200°/s²
- Motor B target → 180.0° with vmax 120°/s, rotation mode `0` (shortest), accel 300°/s²
- Motor C target → 10.0° with vmax 150°/s and accel 200°/s²
- Motor D target → 180.0° with vmax 120°/s, rotation mode `0` (shortest), accel 300°/s²

After reception the Arduino applies the parameters and replies with an `S:` frame describing the applied state.

---
**Units and conversions**
- Positions reported via the serial API: degrees (°). Internally the firmware uses steps per revolution; conversions are handled by the firmware.
- Speeds: degrees per second (°/s).
- Accelerations: degrees per second squared (°/s²).

---
**Build & deploy (PlatformIO)**
Prerequisites: PlatformIO installed (for example via the VS Code PlatformIO extension) and the board connected.

- Build:
```powershell
cd C:\perso\moving_speaker
platformio run
```

- Upload to the board:
```powershell
platformio run --target upload
```

- Open the serial monitor (115200 baud):
```powershell
platformio device monitor -b 115200
```

---
**Example: send a command from Windows (Python + pyserial)**
Install pyserial:
```powershell
pip install pyserial
```
Send a simple command:
```powershell
python -c "import serial, time; s=serial.Serial('COM3',115200,timeout=1); time.sleep(2); s.write(b'10.0,150.0,200.0,180.0,120.0,0,300.0,10.0,150.0,200.0,180.0,120.0,0,300.0\n'); print(s.readline().decode()); s.close()"
```
(adjust `COM3` to your port)

---
**Quick troubleshooting**
- No response: check serial port and baud rate (115200).
- `E:Invalid frame...`: verify there are exactly 7 fields (6 commas) and the line ends with `\n`.
- Unexpected values: check the ranges printed in the `I:` frame at startup.

---
**Key source files**
- `src/main.cpp` — main logic, serial parsing, frame formats
- `src/stepper.h` / `src/stepper.cpp` — unit conversions, limits and rotation modes
- `src/timer.h` / `src/timer.cpp` — timer configuration and ISRs
---
 
---
**Simulator (PC)**
- A PC simulator with a GUI is included in the `moving_speaker_sim` directory. It connects to the Arduino over serial, lets you send setpoints and displays motor status.
- Read the simulator documentation here: `moving_speaker_sim/README.md`

---
**Credit**
- Concept : Armand Lesecq https://www.armandlesecq.com
- Mechanics : osoba_hrvoje https://hrvojespudic.net
- Software : Thomas Faure https://www.detourner.fr