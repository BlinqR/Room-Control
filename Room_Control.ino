#define WIFI_SSID     "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

#include <ESP32Servo.h>
#include <IRremote.h>

#include <WiFi.h>
#include <WebServer.h>

#include <time.h>

#include <Preferences.h>
Preferences prefs;

//Lights servo motor
Servo lightServo;
Servo acServo;


#define motionSensor 35
#define lightServoPin 33
#define button 21
#define IRrecieverPin 34
#define ACServoPin 32

//7 Segment pins
#define A 5
#define B 17
#define C 16
#define D 0
#define E 4
#define F 18
#define G 19

//Debugging LEDs
#define GLED 2
#define RLED 15

bool lightOn;
bool inRoom;
bool acOn;

//WIFI
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// NTP config — Cairo is UTC+3 during DST (April–October)
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 10800;
const int daylightOffset_sec = 0;

bool timeSynced = false;

// Schedule settings
int scheduleHour = 0;
int scheduleMinute = 0;
bool scheduleEnabled = true;

// Prevent re-firing within the same minute
bool scheduleFiredThisMinute = false;
int lastFiredMinute = -1;

WebServer server(80);

String webpage() {
  String html = "<!DOCTYPE html><html>";
  html += "<head><meta name='viewport' content='width=device-width, initial-scale=1'>";

  html += "<style>";
  html += "body { text-align:center; font-family: Arial; }";
  html += "button { padding:20px; font-size:20px; margin:10px; color:white; border:none; border-radius:8px; }";

  html += ".on { background-color: green; }";
  html += ".off { background-color: red; }";
  html += ".acButton { background-color: blue; }";

  html += "</style></head><body>";

  html += "<h1>Smart Room Control</h1>";

  // Light button (dynamic color)
  html += "<form action='/lightToggle' method='POST'>";
  html += "<button type='submit' class='";
  html += (lightOn ? "on" : "off");
  html += "'>";
  html += (lightOn ? "Light ON" : "Light OFF");
  html += "</button></form>";

  // AC button (always blue)
  html += "<form action='/acToggle' method='POST'>";
  html += "<button type='submit' class='acButton'>Toggle AC</button>";
  html += "</form>";

  // Schedule
  html += "<p style='margin:0'>Lights on scheduled for:</p>";
  html += "<h2 style='margin-top:5px'>" + formatTime12h() + "</h2>";

  html += "<form action='/setSchedule' method='POST'>";
  html += "Auto-on time: ";
  html += "<input type='number' name='hour' min='1' max='12' value='" + String(scheduleHour % 12 == 0 ? 12 : scheduleHour % 12) + "' style='width:50px'> : ";
  html += "<input type='number' name='minute' min='0' max='59' value='" + String(scheduleMinute) + "' style='width:50px'> ";
  html += "<select name='period'>";
  html += "<option value='AM'" + String(scheduleHour < 12 ? " selected" : "") + ">AM</option>";
  html += "<option value='PM'" + String(scheduleHour >= 12 ? " selected" : "") + ">PM</option>";
  html += "</select> ";
  html += "<button type='submit' style='background:purple;color:white;padding:10px;border:none;border-radius:6px'>Set</button>";
  html += "</form>";

  html += "</body></html>";
  return html;
}

void handleRoot() {
  server.send(200, "text/html", webpage());
}

void handleLightToggle() {
  toggleLight();
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleLightOn() {
  turnOnLight();
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleLightOff() {
  turnOffLight();
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleACToggle() {
  toggleAC();
  server.sendHeader("Location", "/");
  server.send(303);
}

bool wasConnected = false;

void WIFIHandle() {
  bool connected = (WiFi.status() == WL_CONNECTED);
  if (connected && !wasConnected)  // just connected
  {
    wasConnected = true;
    digitalWrite(RLED, 0);
    for (int i = 0; i < 2; i++) {
      digitalWrite(GLED, 1);
      delay(100);
      digitalWrite(GLED, 0);
      delay(100);
    }

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    // Wait up to 2 seconds for NTP response
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 2000)) {
      timeSynced = true;
      Serial.printf("Time synced: %02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min);
    }
  }

  if (!connected && wasConnected)  // just disconnected
  {
    wasConnected = false;
    for (int i = 0; i < 2; i++) {
      digitalWrite(RLED, 1);
      delay(100);
      digitalWrite(RLED, 0);
      delay(100);
    }
    WiFi.begin(ssid, password);  // attempt reconnect
  }
}

void handleSetSchedule() {
  if (server.hasArg("hour") && server.hasArg("minute") && server.hasArg("period")) {
    int h = server.arg("hour").toInt();
    String period = server.arg("period");
    scheduleMinute = server.arg("minute").toInt();

    if (period == "AM") {
      scheduleHour = (h == 12) ? 0 : h;
    } else {
      scheduleHour = (h == 12) ? 12 : h + 12;
    }
    scheduleEnabled = true;

    prefs.begin("schedule", false);
    prefs.putInt("hour", scheduleHour);
    prefs.putInt("minute", scheduleMinute);
    prefs.end();
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void setup() {
  pinMode(A, OUTPUT);
  pinMode(B, OUTPUT);
  pinMode(C, OUTPUT);
  pinMode(D, OUTPUT);
  pinMode(E, OUTPUT);
  pinMode(F, OUTPUT);
  pinMode(G, OUTPUT);
  pinMode(motionSensor, INPUT);
  pinMode(button, INPUT);

  pinMode(GLED, OUTPUT);
  pinMode(RLED, OUTPUT);

  digitalWrite(RLED, 1);

  Serial.begin(9600);
  WiFi.begin(ssid, password);
  WIFIHandle();

  // Routes
  server.on("/", handleRoot);
  server.on("/lightToggle", HTTP_POST, handleLightToggle);
  server.on("/acToggle", HTTP_POST, handleACToggle);
  server.on("/lightOn", HTTP_POST, handleLightOn);
  server.on("/lightOff", HTTP_POST, handleLightOff);
  server.on("/setSchedule", HTTP_POST, handleSetSchedule);
  server.begin();

  // Load Time
  prefs.begin("schedule", false);
  scheduleHour = prefs.getInt("hour", 7);  // 7 is the default if nothing saved yet
  scheduleMinute = prefs.getInt("minute", 0);
  prefs.end();

  acServo.attach(ACServoPin);
  lightServo.attach(lightServoPin);
  IrReceiver.begin(IRrecieverPin);

  inRoom = 1;
  lightOn = 0;
  acOn = 0;
  lightServo.write(110);
  acServo.write(110);
}

void loop() {
  server.handleClient();
  WIFIHandle();
  systemOn();
  checkSchedule();
}

//System function
void systemOn() {
  //If reciever detected
  if (IrReceiver.decode()) {
    if (IrReceiver.decodedIRData.decodedRawData == 0xBA45FF00) {
      if (!lightOn) {
        turnOnLight();
      } else {
        turnOffLight();
      }
    } else if (IrReceiver.decodedIRData.decodedRawData == 0xB847FF00)
      toggleAC();
    delay(100);
    IrReceiver.resume();
  }

  // If button is pressed change mode to not in room and turn off the lights and issue a 9 second countdown
  if (digitalRead(button)) {
    inRoom = 0;
    turnOffLight();

    for (int countdown = 9; countdown >= 0; countdown--) {
      printNum(countdown);
      delay(1000);
    }

    printNum(-1);  // clear the 7 segment
  }

  // Motion sensing logic
  if (!inRoom && digitalRead(motionSensor)) {
    inRoom = 1;
    turnOnLight();
  }
}

// Checks if time to turn on lights
void checkSchedule() {
  if (!timeSynced || !scheduleEnabled) return;

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 0)) return;  // non-blocking, 0ms timeout

  int currentMinute = timeinfo.tm_hour * 60 + timeinfo.tm_min;
  int targetMinute = scheduleHour * 60 + scheduleMinute;

  // Reset the fired flag once we're past the target minute
  if (currentMinute != targetMinute) {
    scheduleFiredThisMinute = false;
  }

  if (currentMinute == targetMinute && !scheduleFiredThisMinute) {
    Serial.println("Schedule triggered — turning on light");
    turnOnLight();
    scheduleFiredThisMinute = true;
  }
}

// Helper to convert stored 24h to 12h for display
String formatTime12h() {
  int h = scheduleHour;
  String period = h >= 12 ? "PM" : "AM";
  if (h == 0) h = 12;
  else if (h > 12) h -= 12;
  return String(h) + ":" + (scheduleMinute < 10 ? "0" : "") + String(scheduleMinute) + " " + period;
}

//Toggles room lights
void toggleLight() {
  if (!lightOn) {
    turnOnLight();
  } else {
    turnOffLight();
  }
}

//Turns on room lights
void turnOnLight() {
  lightServo.write(75);
  delay(200);
  lightServo.write(110);
  lightOn = 1;
}

//Turns off room lights
void turnOffLight() {
  lightServo.write(145);
  delay(200);
  lightServo.write(110);
  lightOn = 0;
}

//Toggles AC
void toggleAC() {
  acServo.write(75);
  delay(100);
  acServo.write(110);
  delay(200);
  acServo.write(75);
  delay(100);
  acServo.write(110);
  acOn = !acOn;
}

//Prints number on 7 segment
void printNum(int num) {
  switch (num) {
    case 0:
      digitalWrite(A, 1);
      digitalWrite(B, 1);
      digitalWrite(C, 1);
      digitalWrite(D, 1);
      digitalWrite(E, 1);
      digitalWrite(F, 1);
      digitalWrite(G, 0);
      break;
    case 1:
      digitalWrite(A, 0);
      digitalWrite(B, 1);
      digitalWrite(C, 1);
      digitalWrite(D, 0);
      digitalWrite(E, 0);
      digitalWrite(F, 0);
      digitalWrite(G, 0);
      break;
    case 2:
      digitalWrite(A, 1);
      digitalWrite(B, 1);
      digitalWrite(C, 0);
      digitalWrite(D, 1);
      digitalWrite(E, 1);
      digitalWrite(F, 0);
      digitalWrite(G, 1);
      break;
    case 3:
      digitalWrite(A, 1);
      digitalWrite(B, 1);
      digitalWrite(C, 1);
      digitalWrite(D, 1);
      digitalWrite(E, 0);
      digitalWrite(F, 0);
      digitalWrite(G, 1);
      break;
    case 4:
      digitalWrite(A, 0);
      digitalWrite(B, 1);
      digitalWrite(C, 1);
      digitalWrite(D, 0);
      digitalWrite(E, 0);
      digitalWrite(F, 1);
      digitalWrite(G, 1);
      break;
    case 5:
      digitalWrite(A, 1);
      digitalWrite(B, 0);
      digitalWrite(C, 1);
      digitalWrite(D, 1);
      digitalWrite(E, 0);
      digitalWrite(F, 1);
      digitalWrite(G, 1);
      break;
    case 6:
      digitalWrite(A, 1);
      digitalWrite(B, 0);
      digitalWrite(C, 1);
      digitalWrite(D, 1);
      digitalWrite(E, 1);
      digitalWrite(F, 1);
      digitalWrite(G, 1);
      break;
    case 7:
      digitalWrite(A, 1);
      digitalWrite(B, 1);
      digitalWrite(C, 1);
      digitalWrite(D, 0);
      digitalWrite(E, 0);
      digitalWrite(F, 0);
      digitalWrite(G, 0);
      break;
    case 8:
      digitalWrite(A, 1);
      digitalWrite(B, 1);
      digitalWrite(C, 1);
      digitalWrite(D, 1);
      digitalWrite(E, 1);
      digitalWrite(F, 1);
      digitalWrite(G, 1);
      break;
    case 9:
      digitalWrite(A, 1);
      digitalWrite(B, 1);
      digitalWrite(C, 1);
      digitalWrite(D, 1);
      digitalWrite(E, 0);
      digitalWrite(F, 1);
      digitalWrite(G, 1);
      break;
    default:
      digitalWrite(A, 0);
      digitalWrite(B, 0);
      digitalWrite(C, 0);
      digitalWrite(D, 0);
      digitalWrite(E, 0);
      digitalWrite(F, 0);
      digitalWrite(G, 0);
      break;
  }
}

// Call this function to get hex code of the remote button
void read() {
  if (IrReceiver.decode()) {
    Serial.print("Data: ");
    Serial.println(IrReceiver.decodedIRData.decodedRawData, HEX);
    delay(100);
    IrReceiver.resume();
  }
}



/*
Power       BA45FF00
Vol+        B946FF00
Func/Stop   B847FF00
Previous    BB44FF00
Play        BF40FF00
Next        BC43FF00
Down        F807FF00
Vol-        EA15FF00
Up          F609FF00
0           E916FF00
EQ          E619FF00
ST/REPT     F20DFF00
1           F30CFF00
2           E718FF00
3           A15EFF00
4           F708FF00
5           E31CFF00
6           A55AFF00
7           BD42FF00
8           AD52FF00
9           B54AFF00
*/
