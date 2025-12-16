# Moving Speaker (Arduino)

This repository contains the Arduino firmware for the "Moving Speaker" project (control of two stepper motors to aim a speaker). The main firmware is in `src/main.cpp`.

**Project Website**  
[View the project on Armand Lesecq's website](https://www.armandlesecq.com/installations/endroits-d-un-lieu)

**Images**  
![Image 1](https://www.armandlesecq.com/content/4-installations/1-endroits-d-un-lieu/armandlesecq_moving-loudspeaker_hyperdirective-speaker_sound-spatialisation.jpg)  
![Image 2](https://www.armandlesecq.com/content/4-installations/1-endroits-d-un-lieu/a7401822.jpg)

**Hardware**
- The firmware is developed and tested for the Arduino Nano platform. 

**Purpose**
- Drive two stepper motors (A and B) with control over position, speed and acceleration.
- Communicate with a PC interface over a serial link (115200 baud) to receive setpoints and return status.

**Demo**
You can download demo/demo.mp4 video file


**Key firmware files**
- Main application: `src/main.cpp`
- Timer / ISR configuration: `src/timer.h`, `src/timer.cpp`
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
	- `I:` followed by a line with 12 comma-separated values (no extra prefix) that describe the limits and ranges for motors A then B.

	Order of the 12 values (comma-separated):
	- (1) motor A, min position in degree 
	- (2)  motor A, max position in degree
	- (3)  motor A, min speed in °/s
	- (4) motor A, max speed in °/s
	- (5) motor A, min accelleration in °/s^2
	- (6) motor A, max accelleration in °/s^2
	- (7-12) motor B, (like motor A)

	Example (console):
	I:
	-90,90,0.1,20,1,100,0,359.99,0.1,20,1,100

2) Periodic status frames (`P:`)
- Emitted approximately every 100 ms (controlled in the main loop).
- Format:
	P:isRunningA,positionA_deg,speedA_degPerSec,isRunningB,positionB_deg_modulo,speedB_degPerSec

	Field details:
	- `isRunningA`: `0` or `1` (motor A is moving)
	- `positionA_deg`: current position in degrees (may include two decimals)
	- `speedA_degPerSec`: current speed in degrees/s
	- `isRunningB`: `0` or `1` (motor B is moving)
	- `positionB_deg_modulo`: B's position normalized modulo 360° (0..360)
	- `speedB_degPerSec`: current speed in degrees/s

	Example:
	P:1,12.34,5.00,0,270.00,0.00

3) Confirmation frames after a command is received (`S:`)
- When the Arduino receives and accepts a command frame, it replies with:
	S:isRunningA,targetA_deg,maxSpeedA_deg,accelA_degPerSec,isRunningB,targetB_deg,maxSpeedB_deg,accelB_degPerSec

	Example:
	S:1,45.00,17.00,50.00,0,90.00,17.00,50.00

4) Error frames (`E:`)
- Format error (wrong number of fields):
	E:Invalid frame: wrong number of fields

---
**Command format (PC → Arduino)**

The firmware expects a single CSV line (no prefix) containing exactly 7 fields separated by commas, followed by a newline (`\n`).

Order of the 7 fields:
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

Notes:
- All fields are ASCII decimal numbers; floating point values are accepted where relevant.
- The line must contain exactly 6 commas (7 fields). Otherwise the Arduino will return an `E:` error frame.

Command example (terminated by `\n`):
```
10.0,150.0,200.0,180.0,120.0,0,300.0
```
This means:
- Motor A target → 10.0° with vmax 150°/s and accel 200°/s²
- Motor B target → 180.0° with vmax 120°/s, rotation mode `0` (shortest), accel 300°/s²

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
python -c "import serial, time; s=serial.Serial('COM3',115200,timeout=1); time.sleep(2); s.write(b'10.0,150.0,200.0,180.0,120.0,0,300.0\n'); print(s.readline().decode()); s.close()"
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