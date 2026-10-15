
  Project: AI and IoT-Based Smart Irrigation System
  Board: ESP32 DevKit
  IDE: Arduino IDE

  Features:
  1. Reads an analog soil moisture sensor.
  2. Automatically controls a pump through a relay.
  3. Uses hysteresis to reduce rapid relay switching.
  4. Connects to Wi-Fi and provides a monitoring webpage.
  5. Provides a JSON status endpoint.

  

#include <WiFi.h>
#include <WebServer.h>

// -------- Wi-Fi configuration --------
// Replace these with your Wi-Fi credentials.
// Leave empty to run without Wi-Fi.
const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// -------- Hardware pins --------
const int SOIL_SENSOR_PIN = 34;
const int RELAY_PIN = 26;

// Many relay modules are active-LOW.
// Change to false if your relay is active-HIGH.
const bool RELAY_ACTIVE_LOW = true;

// -------- Sensor calibration --------
// EXAMPLE VALUES ONLY. Calibrate using your own sensor.
const int DRY_THRESHOLD = 2800;
const int WET_THRESHOLD = 2200;

// Sampling interval
const unsigned long SAMPLE_INTERVAL_MS = 1000;

// -------- Global variables --------
WebServer server(80);

int soilRaw = 0;
bool pumpOn = false;
bool sensorRead = false;
bool wifiConnected = false;

unsigned long lastSampleTime = 0;

// -------- Relay control --------
void setPump(bool turnOn) {
  pumpOn = turnOn;

  if (RELAY_ACTIVE_LOW) {
    digitalWrite(RELAY_PIN, turnOn ? LOW : HIGH);
  } else {
    digitalWrite(RELAY_PIN, turnOn ? HIGH : LOW);
  }

  Serial.println(turnOn ? "Pump status: ON" : "Pump status: OFF");
}

// -------- Read soil sensor --------
void readSoilSensor() {
  soilRaw = analogRead(SOIL_SENSOR_PIN);
  sensorRead = true;

  Serial.print("Soil sensor ADC value: ");
  Serial.println(soilRaw);

  // Automatic control with hysteresis.
  // Assumes high readings mean dry soil.
  if (!pumpOn && soilRaw >= DRY_THRESHOLD) {
    setPump(true);
  } else if (pumpOn && soilRaw <= WET_THRESHOLD) {
    setPump(false);
  }
}

// -------- HTML monitoring page --------
void handleRoot() {
  String page = R"HTML(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta http-equiv="refresh" content="5">
  <title>Smart Irrigation Monitor</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      max-width: 650px;
      margin: 30px auto;
      padding: 16px;
      background: #f4f7f8;
      color: #222;
    }
    .card {
      background: white;
      padding: 20px;
      margin: 14px 0;
      border-radius: 10px;
      box-shadow: 0 2px 8px #0001;
    }
    h1 { color: #176b45; }
    .value { font-size: 25px; font-weight: bold; }
  </style>
</head>
<body>
  <h1>Smart Irrigation System</h1>
  <div class="card">
    <h2>Soil Sensor</h2>
    <p>Raw ADC reading:</p>
    <p class="value" id="soil">Loading...</p>
    <p>Higher readings are assumed to indicate drier soil.</p>
  </div>
  <div class="card">
    <h2>Pump Status</h2>
    <p class="value" id="pump">Loading...</p>
    <p>Control mode: Automatic</p>
  </div>
  <div class="card">
    <h2>Connection</h2>
    <p id="wifi">Checking...</p>
  </div>
  <script>
    fetch('/status')
      .then(response => response.json())
      .then(data => {
        document.getElementById('soil').textContent =
          data.sensor_available ? data.soil_raw : 'No reading';
        document.getElementById('pump').textContent =
          data.pump_on ? 'ON' : 'OFF';
        document.getElementById('wifi').textContent =
          data.wifi_connected ? 'Connected' : 'Not connected';
      })
      .catch(() => {
        document.getElementById('wifi').textContent =
          'Unable to read status';
      });
  </script>
</body>
</html>
)HTML";

  server.send(200, "text/html", page);
}

// -------- JSON status endpoint --------
void handleStatus() {
  String json = "{";
  json += "\"soil_raw\":" + String(soilRaw) + ",";
  json += "\"sensor_available\":";
  json += sensorRead ? "true" : "false";
  json += ",";
  json += "\"pump_on\":";
  json += pumpOn ? "true" : "false";
  json += ",";
  json += "\"wifi_connected\":";
  json += wifiConnected ? "true" : "false";
  json += "}";

  server.send(200, "application/json", json);
}

// -------- Wi-Fi setup --------
void connectWiFi() {
  if (strlen(WIFI_SSID) == 0 ||
      String(WIFI_SSID) == "YOUR_WIFI_NAME") {
    Serial.println("Wi-Fi not configured. Running locally.");
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to Wi-Fi");

  unsigned long startTime = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - startTime < 15000) {
    delay(300);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.print("Connected. Open this IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Wi-Fi connection failed. Irrigation remains local.");
  }
}

// -------- Setup --------
void setup() {
  Serial.begin(115200);

  // Initialize relay and turn pump OFF immediately.
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, RELAY_ACTIVE_LOW ? HIGH : LOW);
  pumpOn = false;

  pinMode(SOIL_SENSOR_PIN, INPUT);
  analogReadResolution(12);

  Serial.println();
  Serial.println("Smart Irrigation System Starting...");

  // Read sensor before enabling automatic control.
  readSoilSensor();

  connectWiFi();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/status", HTTP_GET, handleStatus);
  server.begin();

  Serial.println("Web server started.");
}

// -------- Main loop --------
void loop() {
  if (millis() - lastSampleTime >= SAMPLE_INTERVAL_MS) {
    lastSampleTime = millis();
    readSoilSensor();
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    server.handleClient();
  } else {
    wifiConnected = false;
  }
}
