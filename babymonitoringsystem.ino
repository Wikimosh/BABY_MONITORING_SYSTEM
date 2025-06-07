#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

// --- PIN DEFINITIONS FOR 30-PIN ESP32 DEV KIT ---
#define SOUND_SENSOR_PIN 34
#define MOISTURE_SENSOR_PIN 35
#define SERVO_ROCK_PIN 18
#define SERVO_TOY_PIN 19
#define ALERT_PIN 23 // Digital output pin for external alert

// --- SERVO SETUP ---
Servo rockerServo;
Servo toyServo;

// --- ACCESS POINT SETUP ---
const char* ssid = "BabyMonitorAPP";
const char* password = "12345678";
WebServer server(80);

// --- STATUS VARIABLES ---
bool crying = false;
bool wet = false;
bool manualMode = false;
bool rockingActive = false;

unsigned long lastSoundTime = 0;
const int soundThreshold = 700;
const int moistureThreshold = 2500;

void setup() {
  Serial.begin(9600);

  pinMode(ALERT_PIN, OUTPUT);
  digitalWrite(ALERT_PIN, LOW);

  // Attach servos
  rockerServo.attach(SERVO_ROCK_PIN);
  toyServo.attach(SERVO_TOY_PIN);
  rockerServo.write(90);
  toyServo.write(90);

  // Set up Access Point
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.println("Access Point started");
  Serial.print("IP address: ");
  Serial.println(IP);

  // Start web server
  server.on("/", handleRoot);
  server.on("/toggleMode", []() {
    manualMode = !manualMode;
    rockingActive = false;
    server.send(200, "text/plain", manualMode ? "Manual Mode" : "Auto Mode");
  });
  server.on("/rockOn", []() {
    rockingActive = true;
    server.send(200, "text/plain", "Rocking ON");
  });
  server.on("/rockOff", []() {
    rockingActive = false;
    rockerServo.write(90);
    server.send(200, "text/plain", "Rocking OFF");
  });
  server.begin();
}

void loop() {
  server.handleClient();

  int soundValue = analogRead(SOUND_SENSOR_PIN);
  int moistureValue = analogRead(MOISTURE_SENSOR_PIN);

  if (!manualMode) {
    if (soundValue > soundThreshold && !crying) {
      crying = true;
      lastSoundTime = millis();
    }
    if (crying && millis() - lastSoundTime > 3000) {
      rockBaby();
      crying = false;
    }

    if (moistureValue < moistureThreshold && !wet) {
      wet = true;
    } else if (moistureValue >= moistureThreshold) {
      wet = false;
    }
  } else {
    if (rockingActive) {
      rockBabySmooth();
    }
  }

  delay(100);
}

void rockBaby() {
  digitalWrite(ALERT_PIN, HIGH);
  unsigned long startTime = millis();
  while (millis() - startTime < 10000) {
    for (int angle = 90; angle >= 60; angle--) {
      rockerServo.write(angle);
      delay(10);
    }
    for (int angle = 60; angle <= 120; angle++) {
      rockerServo.write(angle);
      delay(10);
    }
    for (int angle = 120; angle >= 90; angle--) {
      rockerServo.write(angle);
      delay(10);
    }
  }
  rockerServo.write(90);
  digitalWrite(ALERT_PIN, LOW);
}

void rockBabySmooth() {
  static bool direction = false;
  static int angle = 90;

  if (direction) {
    angle++;
    if (angle >= 120) direction = false;
  } else {
    angle--;
    if (angle <= 60) direction = true;
  }
  rockerServo.write(angle);
  delay(20);
}

void handleRoot() {
  int soundValue = analogRead(SOUND_SENSOR_PIN);
  int moistureValue = analogRead(MOISTURE_SENSOR_PIN);

  String page = R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
      <meta http-equiv="refresh" content="4">
      <meta name='viewport' content='width=device-width, initial-scale=1.0'>
      <title>Baby Monitor</title>
      <style>
        body { font-family: Arial; background: #f4f4f4; padding: 20px; }
        .container { max-width: 400px; margin: auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 0 10px rgba(0,0,0,0.1); }
        h2 { text-align: center; margin-bottom: 20px; }
        .status { font-size: 1.2em; margin: 10px 0; }
        .label { font-weight: bold; }
        button { padding: 10px; margin: 5px 0; width: 100%; font-size: 1em; border: none; border-radius: 5px; color: white; }
        .mode-button { background-color: %MODE_COLOR%; }
        .on-button { background-color: #007BFF; }
        .off-button { background-color: #dc3545; }
      </style>
      <script>
        if ('Notification' in window && Notification.permission !== 'granted') {
          Notification.requestPermission();
        }
        const crying = "%CRY%" === "Yes";
        const wet = "%WET%" === "Yes";
        if (crying) new Notification("🍼 Baby is crying!");
        if (wet) new Notification("🧷 Diaper is wet!");
      </script>
    </head>
    <body>
      <div class='container'>
        <h2>Baby Monitor</h2>
        <div class='status'><span class='label'>Mode:</span> %MODE%</div>
        <div class='status'><span class='label'>Baby Crying:</span> %CRY%</div>
        <div class='status'><span class='label'>Diaper Wet:</span> %WET%</div>
        <div class='status'><span class='label'>Sound Value:</span> %SOUND%</div>
        <div class='status'><span class='label'>Moisture Value:</span> %MOISTURE%</div>
        <form action="/toggleMode"><button class='mode-button'>Toggle Mode (%MODE%)</button></form>
        %MANUAL_CONTROLS%
      </div>
    </body>
    </html>
  )rawliteral";

  page.replace("%MODE%", manualMode ? "Manual" : "Auto");
  page.replace("%CRY%", crying ? "Yes" : "No");
  page.replace("%WET%", wet ? "Yes" : "No");
  page.replace("%SOUND%", String(soundValue));
  page.replace("%MOISTURE%", String(moistureValue));
  page.replace("%MODE_COLOR%", manualMode ? "#FF5722" : "#6C757D");
  page.replace("%MANUAL_CONTROLS%", manualMode ? "<form action='/rockOn'><button class='on-button'>Rock ON</button></form><form action='/rockOff'><button class='off-button'>Rock OFF</button></form>" : "");

  server.send(200, "text/html", page);
}
