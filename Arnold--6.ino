
#define ENABLE_USER_AUTH
#define ENABLE_DATABASE

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <FirebaseClient.h>
#include <DHT.h>
#include <WebServer.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <time.h>

// ============================================================
// FIREBASE CONFIGURATION
// ============================================================


#define API_KEY "AIzaSyB9-FYcrAgyAPitLZrJpxyMZs27TEtc9AQ"
#define USER_EMAIL "arnoldeusala76@gmail.com"
#define USER_PASSWORD "Astrlogo@19"

#define DATABASE_URL "https://arnold-28517-default-rtdb.europe-west1.firebasedatabase.app/"

// ============================================================
// PIN CONFIGURATION
// ============================================================

#define DHTPIN 4
#define DHTTYPE DHT11
#define RELAY1_PIN 26
#define RELAY2_PIN 27
#define RELAY_ACTIVE_LOW true

DHT dht(DHTPIN, DHTTYPE);

// ============================================================
// WIFI MANAGER
// ============================================================

const char* AP_SSID = "ARNOLD_WIFI_MANAGER";

Preferences preferences;
WebServer server(80);

String savedSSID = "";
String savedPassword = "";
String savedIP = "";
String savedGateway = "";

// ============================================================
// FIREBASE OBJECTS
// ============================================================

WiFiClientSecure ssl_client;
using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client);

FirebaseApp app;
RealtimeDatabase Database;
UserAuth user_auth(API_KEY, USER_EMAIL, USER_PASSWORD);

// ============================================================
// SENSOR / RELAY / TIME
// ============================================================

float temperature = NAN;
float humidity = NAN;

unsigned long previousSensorMillis = 0;
const unsigned long SENSOR_INTERVAL = 10000;

bool relay1State = false;
bool relay2State = false;

const char* NTP_SERVER = "pool.ntp.org";
const long GMT_OFFSET_SECONDS = 8 * 3600;
const int DAYLIGHT_OFFSET_SECONDS = 0;

// ============================================================
// WIFI MANAGER HTML
// ============================================================

String wifiManagerPage() {
  return R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>CORE // WiFi Manager</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{min-height:100vh;background:#080d14;color:#dbeafe;font-family:"Segoe UI",Arial,sans-serif;display:flex;justify-content:center;align-items:center;padding:20px}
.grid{position:fixed;inset:0;z-index:-1;background-image:linear-gradient(rgba(14,165,233,.035) 1px,transparent 1px),linear-gradient(90deg,rgba(14,165,233,.035) 1px,transparent 1px);background-size:40px 40px}
.card{width:100%;max-width:520px;background:#111923;border:1px solid #172033;padding:35px;box-shadow:0 0 50px rgba(0,0,0,.35)}
.brand{color:#22d3ee;font-family:monospace;font-size:12px;letter-spacing:3px;margin-bottom:10px}
h1{color:#f8fafc;font-size:32px;margin-bottom:8px}
.subtitle{color:#64748b;font-family:monospace;font-size:10px;letter-spacing:2px;margin-bottom:30px}
label{display:block;color:#64748b;font-family:monospace;font-size:10px;letter-spacing:2px;margin-bottom:8px}
input{width:100%;padding:14px;margin-bottom:20px;background:#050a10;color:#f8fafc;border:1px solid #263449;outline:none;font-family:monospace}
input:focus{border-color:#22d3ee;box-shadow:0 0 0 1px #22d3ee}
button{width:100%;padding:15px;background:#06202a;color:#f4fafb;border:1px solid #22d3ee;font-family:monospace;font-weight:bold;letter-spacing:2px;cursor:pointer}
button:hover{background:#083746}
.note{margin-top:20px;color:#64748b;font-family:monospace;font-size:10px;line-height:1.7}
</style>
</head>
<body>
<div class="grid"></div>
<div class="card">
<div class="brand">CORE // ESP32</div>
<h1>WiFi Manager</h1>
<div class="subtitle">ARNOLD EUSALA ENVIRONMENT MONITOR</div>
<form method="POST" action="/save-wifi">
<label>WIFI SSID</label>
<input type="text" name="ssid" placeholder="Enter WiFi name" required>
<label>WIFI PASSWORD</label>
<input type="password" name="pass" placeholder="Enter WiFi password">
<label>STATIC IP</label>
<input type="text" name="ip" placeholder="192.168.1.100">
<label>GATEWAY</label>
<input type="text" name="gateway" placeholder="192.168.1.1">
<button type="submit">SAVE &amp; RESTART</button>
</form>
<div class="note">Leave STATIC IP and GATEWAY empty if you want the ESP32 to use DHCP.</div>
</div>
</body>
</html>
)rawliteral";
}

// ============================================================
// LOAD WIFI SETTINGS
// ============================================================

void loadWiFiSettings() {
  preferences.begin("arnold-wifi", true);
  savedSSID = preferences.getString("ssid", "");
  savedPassword = preferences.getString("pass", "");
  savedIP = preferences.getString("ip", "");
  savedGateway = preferences.getString("gateway", "");
  preferences.end();
}

// ============================================================
// START OPEN ESP32 ACCESS POINT
// ============================================================

void startAccessPoint() {
  Serial.println();
  Serial.println("Starting Arnold WiFi Manager...");
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID);

  Serial.println("WiFi Manager started.");
  Serial.print("WiFi name: ");
  Serial.println(AP_SSID);
  Serial.println("Password: NONE");
  Serial.print("WiFi Manager URL: http://");
  Serial.println(WiFi.softAPIP());
}

// ============================================================
// CONNECT TO SAVED ROUTER WIFI
// ============================================================

void connectToSavedWiFi() {
  loadWiFiSettings();

  if (savedSSID.isEmpty()) {
    Serial.println("No saved WiFi credentials. Configure using WiFi Manager.");
    return;
  }

  WiFi.mode(WIFI_AP_STA);

  if (!savedIP.isEmpty() && !savedGateway.isEmpty()) {
    IPAddress localIP, gatewayIP;
    if (localIP.fromString(savedIP) && gatewayIP.fromString(savedGateway)) {
      IPAddress subnet(255, 255, 255, 0);
      IPAddress dns(8, 8, 8, 8);
      if (WiFi.config(localIP, gatewayIP, subnet, dns)) {
        Serial.println("Static IP configured.");
      } else {
        Serial.println("Static IP configuration failed; continuing connection attempt.");
      }
    } else {
      Serial.println("Invalid static IP or gateway; using DHCP.");
    }
  }

  Serial.print("Connecting to router WiFi: ");
  Serial.println(savedSSID);

  WiFi.begin(savedSSID.c_str(), savedPassword.c_str());

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 20000) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Router WiFi connected!");
    Serial.print("Dashboard URL: http://");
    Serial.println(WiFi.localIP());

    configTime(GMT_OFFSET_SECONDS, DAYLIGHT_OFFSET_SECONDS, NTP_SERVER);
  } else {
    Serial.println("Router WiFi connection failed.");
    Serial.println("WiFi Manager remains available at http://192.168.4.1");
  }
}

// ============================================================
// FILE SERVING HELPERS
// ============================================================

String contentTypeFor(const String& path) {
  if (path.endsWith(".html")) return "text/html";
  if (path.endsWith(".css")) return "text/css";
  if (path.endsWith(".js")) return "application/javascript";
  if (path.endsWith(".json")) return "application/json";
  if (path.endsWith(".png")) return "image/png";
  if (path.endsWith(".jpg") || path.endsWith(".jpeg")) return "image/jpeg";
  if (path.endsWith(".svg")) return "image/svg+xml";
  if (path.endsWith(".ico")) return "image/x-icon";
  return "text/plain";
}

bool serveLittleFSFile(const String& path) {
  if (!LittleFS.exists(path)) {
    server.send(404, "text/plain", "File not found: " + path);
    return false;
  }

  File file = LittleFS.open(path, "r");
  if (!file) {
    server.send(500, "text/plain", "Unable to open file.");
    return false;
  }

  server.streamFile(file, contentTypeFor(path));
  file.close();
  return true;
}

void handleRoot() {
  if (WiFi.status() == WL_CONNECTED && LittleFS.exists("/index.html")) {
    serveLittleFSFile("/index.html");
  } else {
    server.send(200, "text/html", wifiManagerPage());
  }
}

void handleWiFiManager() {
  server.send(200, "text/html", wifiManagerPage());
}

// ============================================================
// SAVE WIFI SETTINGS
// ============================================================

void handleSaveWiFi() {
  if (!server.hasArg("ssid") || server.arg("ssid").length() == 0) {
    server.send(400, "text/plain", "WiFi SSID is required.");
    return;
  }

  String newSSID = server.arg("ssid");
  String newPassword = server.arg("pass");
  String newIP = server.arg("ip");
  String newGateway = server.arg("gateway");

  newSSID.trim();
  newIP.trim();
  newGateway.trim();

  if ((newIP.length() == 0) != (newGateway.length() == 0)) {
    server.send(400, "text/plain",
      "Enter both Static IP and Gateway, or leave both empty.");
    return;
  }

  if (newIP.length() > 0) {
    IPAddress testIP, testGateway;
    if (!testIP.fromString(newIP) || !testGateway.fromString(newGateway)) {
      server.send(400, "text/plain", "Invalid IP address or gateway.");
      return;
    }
  }

  preferences.begin("arnold-wifi", false);
  preferences.putString("ssid", newSSID);
  preferences.putString("pass", newPassword);
  preferences.putString("ip", newIP);
  preferences.putString("gateway", newGateway);
  preferences.end();

  server.send(200, "text/html",
    "<!doctype html><html><body style='background:#080d14;color:#22d3ee;"
    "font-family:Arial;text-align:center;padding:50px'>"
    "<h1>WIFI SETTINGS SAVED</h1>"
    "<p>ESP32 is restarting. Reconnect your phone to your router WiFi, "
    "then open the new ESP32 IP address shown in Serial Monitor.</p>"
    "</body></html>");

  delay(1500);
  ESP.restart();
}

// ============================================================
// RELAY CONTROL
// ============================================================

void setRelay(int relayNumber, bool state) {
  int pin;

  if (relayNumber == 1) {
    pin = RELAY1_PIN;
    relay1State = state;
  } else if (relayNumber == 2) {
    pin = RELAY2_PIN;
    relay2State = state;
  } else {
    return;
  }

  if (RELAY_ACTIVE_LOW) {
    digitalWrite(pin, state ? LOW : HIGH);
  } else {
    digitalWrite(pin, state ? HIGH : LOW);
  }
}

void handleRelayStatus() {
  String json = "{\"LED1\":";
  json += relay1State ? "true" : "false";
  json += ",\"LED2\":";
  json += relay2State ? "true" : "false";
  json += "}";

  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

void handleRelayControl() {
  if (!server.hasArg("relay") || !server.hasArg("state")) {
    server.send(400, "application/json", "{\"success\":false}");
    return;
  }

  int relayNumber = server.arg("relay").toInt();
  String requestedState = server.arg("state");
  requestedState.toLowerCase();

  if ((relayNumber != 1 && relayNumber != 2) ||
      (requestedState != "on" && requestedState != "off")) {
    server.send(400, "application/json", "{\"success\":false}");
    return;
  }

  setRelay(relayNumber, requestedState == "on");
  server.send(200, "application/json", "{\"success\":true}");
}

// ============================================================
// DATE / TIME AND DHT11
// ============================================================

bool getCurrentDateTime(String &dateString, String &timeString) {
  struct tm timeInfo;
  if (!getLocalTime(&timeInfo, 1000)) return false;

  char dateBuffer[12];
  char timeBuffer[12];
  strftime(dateBuffer, sizeof(dateBuffer), "%Y-%m-%d", &timeInfo);
  strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", &timeInfo);

  dateString = String(dateBuffer);
  timeString = String(timeBuffer);
  return true;
}

void readSensor() {
  float newHumidity = dht.readHumidity();
  float newTemperature = dht.readTemperature();

  if (isnan(newHumidity) || isnan(newTemperature)) {
    Serial.println("DHT11 reading failed!");
    return;
  }

  humidity = newHumidity;
  temperature = newTemperature;

  Serial.println();
  Serial.println("========== ARNOLD SENSOR ==========");
  Serial.print("Temperature: ");
  Serial.print(temperature, 1);
  Serial.println(" C");
  Serial.print("Humidity: ");
  Serial.print(humidity, 1);
  Serial.println(" %");
  Serial.println("===================================");
}

// ============================================================
// FIREBASE UPLOAD
// ============================================================

void uploadToFirebase() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Firebase upload skipped: WiFi disconnected.");
    return;
  }

  if (!app.ready()) {
    Serial.println("Firebase is not ready yet.");
    return;
  }

  String dateString, timeString;
  if (!getCurrentDateTime(dateString, timeString)) {
    Serial.println("Time not available.");
    return;
  }

  String databasePath = "/ESP32_Data/" + dateString + "/" + timeString;

  String json = "{\"temperature\":";
  json += String(temperature, 1);
  json += ",\"humidity\":";
  json += String(humidity, 1);
  json += "}";

  Serial.println("Uploading to Firebase...");
  Serial.println(databasePath);

  Database.set<object_t>(aClient, databasePath, object_t(json));
  Serial.println("Firebase upload request sent.");
}

// ============================================================
// WEB SERVER ROUTES
// ============================================================

void setupWebServer() {
  server.on("/", HTTP_GET, handleRoot);

  server.on("/wifi", HTTP_GET, handleWiFiManager);
  server.on("/change-wifi", HTTP_GET, handleWiFiManager);
  server.on("/save-wifi", HTTP_POST, handleSaveWiFi);

  server.on("/relay-status", HTTP_GET, handleRelayStatus);
  server.on("/relay", HTTP_GET, handleRelayControl);

  server.on("/style.css", HTTP_GET, []() {
    serveLittleFSFile("/style.css");
  });

  server.on("/app.js", HTTP_GET, []() {
    serveLittleFSFile("/app.js");
  });

  server.onNotFound([]() {
    String path = server.uri();
    if (path.indexOf("..") >= 0) {
      server.send(400, "text/plain", "Invalid path.");
      return;
    }
    serveLittleFSFile(path);
  });

  server.begin();
  Serial.println("HTTP web server started.");
}

// ============================================================
// SETUP
// ============================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("========================================");
  Serial.println(" ARNOLD ESP32 ENVIRONMENT MONITOR");
  Serial.println("========================================");

  dht.begin();

  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  setRelay(1, false);
  setRelay(2, false);

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed!");
  } else {
    Serial.println("LittleFS mounted.");
    Serial.println("Dashboard files:");
    Serial.println("/index.html");
    Serial.println("/style.css");
    Serial.println("/app.js");
  }

  startAccessPoint();
  connectToSavedWiFi();

  setupWebServer();

  ssl_client.setInsecure();
  Database.url(DATABASE_URL);

  initializeApp(aClient, app, getAuth(user_auth));
  app.getApp<RealtimeDatabase>(Database);

  if (WiFi.status() == WL_CONNECTED) {
    configTime(GMT_OFFSET_SECONDS, DAYLIGHT_OFFSET_SECONDS, NTP_SERVER);
  }

  Serial.println();
  Serial.println("========== ARNOLD READY ==========");
  Serial.print("WiFi Manager: http://");
  Serial.println(WiFi.softAPIP());

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("DASHBOARD IP: http://");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Router WiFi not connected yet.");
    Serial.println("Connect to ARNOLD_WIFI_MANAGER and open http://192.168.4.1");
  }
  Serial.println("===================================");
}

// ============================================================
// LOOP
// ============================================================

void loop() {
  app.loop();
  server.handleClient();

  if (millis() - previousSensorMillis >= SENSOR_INTERVAL) {
    previousSensorMillis = millis();
    readSensor();

    if (!isnan(temperature) && !isnan(humidity)) {
      uploadToFirebase();
    }
  }

  if (!savedSSID.isEmpty() && WiFi.status() != WL_CONNECTED) {
    static unsigned long lastReconnectAttempt = 0;
    if (millis() - lastReconnectAttempt >= 15000) {
      lastReconnectAttempt = millis();
      Serial.println("Trying to reconnect to router WiFi...");
      WiFi.begin(savedSSID.c_str(), savedPassword.c_str());
    }
  }

  static bool previousConnectedState = false;
  bool currentlyConnected = (WiFi.status() == WL_CONNECTED);

  if (currentlyConnected && !previousConnectedState) {
    Serial.println();
    Serial.println("Router WiFi connected!");
    Serial.print("ARNOLD DASHBOARD: http://");
    Serial.println(WiFi.localIP());
    configTime(GMT_OFFSET_SECONDS, DAYLIGHT_OFFSET_SECONDS, NTP_SERVER);
  }

  previousConnectedState = currentlyConnected;
}
