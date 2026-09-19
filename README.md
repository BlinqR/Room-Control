# Room Control

An ESP32-based smart room controller. It switches the room light and the AC using servos, and can be controlled from a web page, an IR remote, a physical button, a motion sensor, or a daily schedule.

## Features

- **Web interface:** toggle the light and the AC from any device on your network, and set the daily auto-on time for the light.
- **IR remote:** toggle the light and the AC with a remote control.
- **Leave-room button:** press it on your way out. The light turns off and a 9-second countdown shows on a 7-segment display. Press the button again during the countdown to cancel and turn the light back on.
- **Motion sensing:** after you leave, the light turns on again when motion is detected (someone coming back in).
- **Scheduled light-on:** the light turns on at a set time every day, using time from an NTP server. The schedule is saved in flash memory and survives power loss.
- **AC toggle handling:** the AC takes about 3.3 seconds to apply a toggle. Toggling again inside that window sends a single servo press instead of a double press, and every toggle restarts the window.
- **Status LEDs:** the red LED is on while WiFi is disconnected. Both LEDs give short blink patterns when WiFi connects or drops.

## Hardware

- ESP32 development board
- 2 servos: one flips the light switch, one presses the AC button
- IR receiver and an IR remote
- Motion sensor (PIR)
- Push button
- 7-segment display (single digit)
- Green and red LEDs

### Pin mapping

| Function | GPIO |
|---|---|
| Motion sensor | 35 |
| Light servo | 33 |
| AC servo | 32 |
| Button | 21 |
| IR receiver | 34 |
| 7-segment A / B / C / D / E / F / G | 5 / 17 / 16 / 0 / 4 / 18 / 19 |
| Green LED (GLED) | 2 |
| Red LED (RLED) | 15 |

## Software setup

1. Install the ESP32 board package in the Arduino IDE.
2. Install these libraries from the Library Manager:
   - **ESP32Servo**
   - **IRremote**
3. Open `Room_Control.ino` and set your WiFi credentials at the top of the file:
   ```cpp
   #define WIFI_SSID     "YOUR_WIFI_SSID"
   #define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
   ```
4. Set your time zone offset if you're not in UTC+3:
   ```cpp
   const long gmtOffset_sec = 10800;   // UTC+3
   const int daylightOffset_sec = 0;   // no automatic DST handling
   ```
5. Upload the sketch and open the Serial Monitor at 9600 baud. Once the board connects to WiFi, use its IP address to open the web interface.

> Don't commit your real WiFi password. Keep the placeholders in the repo, or move the credentials into a separate file that's listed in `.gitignore`.

## Usage

### Web interface

Open the board's IP address in a browser.

| Route | Method | Action |
|---|---|---|
| `/` | GET | Control page |
| `/lightToggle` | POST | Toggle the light |
| `/lightOn` | POST | Turn the light on |
| `/lightOff` | POST | Turn the light off |
| `/acToggle` | POST | Toggle the AC |
| `/setSchedule` | POST | Set the daily auto-on time |

### IR remote

| Remote button | Code | Action |
|---|---|---|
| Power | `BA45FF00` | Toggle the light |
| Func/Stop | `B847FF00` | Toggle the AC |

To use a different remote or button, call the `read()` helper in `loop()`, press the button, and read its code from the Serial Monitor. Then replace the code in `systemOn()`. The codes for the remote used in this project are listed in a comment at the bottom of the sketch.

### Leave-room button

1. Press the button when leaving the room. The light turns off and the display counts down from 9.
2. Press the button again before it reaches 0 to cancel and turn the light back on.
3. After you've left, motion in the room turns the light back on.

## Known limitations

- The countdown blocks the main loop, so the web server, IR remote and schedule don't respond until it finishes.
- The web interface has no authentication, so anyone on your network can use it.
- Motion sensors often hold their output high for a few seconds after the last movement. If the light turns back on right after the countdown ends, add a short `delay()` after the countdown finishes.
- Daylight saving time isn't handled automatically. Change `gmtOffset_sec` by hand when it changes.
