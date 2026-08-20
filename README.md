# Moving Speaker

This repository now hosts two firmware targets for the Moving Speaker project in a single codebase:

- `esp32_4m`: ESP32 firmware for four stepper motors
- `avr_2m`: AVR firmware for two stepper motors

The source tree is split by target under `src/targets/` so each firmware can evolve independently while sharing the same repository, Docker tooling and release flow.

**Project Website**  
[View the project on Armand Lesecq's website](https://www.armandlesecq.com/installations/endroits-d-un-lieu)

**Images**  
![Image 1](https://www.armandlesecq.com/content/4-installations/1-endroits-d-un-lieu/armandlesecq_moving-loudspeaker_hyperdirective-speaker_sound-spatialisation.jpg)  
![Image 2](https://www.armandlesecq.com/content/4-installations/1-endroits-d-un-lieu/a7401822.jpg)

**Hardware**
- `esp32_4m`: Seeed Studio XIAO ESP32-C6
- `avr_2m`: AVR target currently configured for `nanoatmega328new` in PlatformIO

**Purpose**
- Drive either two or four stepper motors depending on the selected target.
- Communicate with a PC interface over a serial link (115200 baud) to receive setpoints and return status.
- The motor movement remains always smooth (managed by timer interrupt TIMER1 IRQ)
- Position and speed setpoints can be sent during movement
- The acceleration setpoint can be modified (taken into account if the motor is stopped)
- Motors A and B are managed independently

**Demo**
You can download demo/demo.mp4 video file


**Key firmware files**
- ESP32 target: `src/targets/esp32_4m/main.cpp`
- AVR target: `src/targets/avr_2m/main.cpp`, `src/targets/avr_2m/timer.h`, `src/targets/avr_2m/timer.cpp`
- Shared motion and protocol code: `src/common/stepper_core.h/.cpp`, `src/common/moving_speaker_protocol.h/.cpp`
- Shared helper: `include/digitalWriteFast.h`

---
**Serial communication settings**
- Baud rate: `115200`
- Format: ASCII lines terminated by newline (`\n`).
- The firmware periodically emits a status frame and responds to commands from the PC.

---
**Frames sent by the firmware**

1) Startup information frames
- The board prints some information at startup:
	- `I: Moving Speaker V2.1 by Détourner`
	- `I:` followed by a line with 12 comma-separated values on AVR or 24 values on ESP32. The values describe the limits and ranges for each configured motor.

	Order of the values (six per motor, comma-separated):
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
	- the same six fields repeat for each additional motor

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

3) State confirmation frames (`S:`)
- The firmware does not send this frame automatically after a command. Send the request `T` followed by `\n` to obtain it:

	T

- The firmware then replies with:
	S:isRunningA,targetA_deg,maxSpeedA_deg,accelA_degPerSec,isRunningB,targetB_deg,maxSpeedB_deg,accelB_degPerSec,isRunningC,targetC_deg,maxSpeedC_deg,accelC_degPerSec,isRunningD,targetD_deg,maxSpeedD_deg,accelD_degPerSec

	Example:
	S:1,45.00,17.00,50.00,0,90.00,17.00,50.00,1,45.00,17.00,50.00,0,90.00,17.00,50.00

4) Error frames (`E:`)
- Format error (wrong number of fields):
	E:Invalid frame: wrong number of fields
- Invalid numeric field:
	E:Invalid frame: invalid numeric field
- Invalid rotation mode:
	E:Invalid frame: invalid rotation mode

---
**Command format (PC -> firmware)**

The serial command format depends on the selected target:

- `esp32_4m` expects 14 CSV fields
- `avr_2m` expects 7 CSV fields

The examples below describe the `esp32_4m` format.

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
- Numeric fields must contain a complete finite number. Values such as `abc`, `nan` or `inf` are rejected.
- Rotation modes must be `0`, `1` or `2`.
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
**Build & deploy**

You can build the firmware in two ways:

1) Native PlatformIO on your machine

Prerequisites: PlatformIO installed (for example via the VS Code PlatformIO extension) and the board connected.

- Build ESP32 target:
```powershell
cd C:\perso\dev\moving_speaker
platformio run -e esp32_4m
```

- Build AVR target:
```powershell
cd C:\perso\dev\moving_speaker
platformio run -e avr_2m
```

- Upload to the selected board:
```powershell
platformio run -e esp32_4m --target upload
```

- Open the serial monitor (115200 baud):
```powershell
platformio device monitor -b 115200
```

2) Dockerized build (recommended for reproducibility)

Use the helper script with an explicit target:

- Build ESP32 target (auto-creates images if missing):
```bat
docker\platformio-docker.bat build esp32_4m
```

- Build AVR target:
```bat
docker\platformio-docker.bat build avr_2m
```

- Show PlatformIO environment details:
```bat
docker\platformio-docker.bat env esp32_4m
```

- Open an interactive shell in a target container:
```bat
docker\platformio-docker.bat shell esp32_4m
```

- Force a full rebuild of image and clean PlatformIO cache:
```bat
docker\platformio-docker.bat rebuild avr_2m
```

- Clean only the PlatformIO cache volume for one target:
```bat
docker\platformio-docker.bat cache-clean esp32_4m
```

Archive and restore image + cache:

- Create both archives for a target:
```bat
docker\platformio-docker.bat archive esp32_4m
```
This creates two files in the `docker` folder:
- `...-image.tar` (Docker image)
- `...-cache.tar` (PlatformIO cache volume)

- Restore from archive:
```bat
docker\platformio-docker.bat load-archive esp32_4m C:\path\to\your-image.tar
```
If the matching `...-cache.tar` is present next to the image archive, it is restored automatically. You can also pass it explicitly:
```bat
docker\platformio-docker.bat load-archive esp32_4m C:\path\to\your-image.tar C:\path\to\your-cache.tar
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
- `E:Invalid frame...`: verify the field count matches the selected target and the line ends with `\n`.
- Unexpected values: check the ranges printed in the `I:` frame at startup.

---
**Key source files**
- `src/targets/esp32_4m/main.cpp` — 4-motor ESP32 application logic
- `src/targets/esp32_4m/main.cpp` — 4-motor ESP32 application and timer-group logic
- `src/targets/avr_2m/main.cpp` — 2-motor AVR application logic
- `src/targets/avr_2m/timer.h` / `src/targets/avr_2m/timer.cpp` — AVR Timer1 configuration and ISRs
- `src/common/stepper_core.h` / `src/common/stepper_core.cpp` — shared stepper implementation
- `src/common/moving_speaker_protocol.h` / `src/common/moving_speaker_protocol.cpp` — shared serial protocol
- `docker/platformio-docker.bat` — per-target Docker build helper
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