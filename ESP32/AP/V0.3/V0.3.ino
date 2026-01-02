#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

WebServer server(80);
Preferences preferences;

// AP temporaire
const char* ap_ssid = "ESP32_Config";
const char* ap_password = "12345678";

// Timer pour AP temporaire
unsigned long apStartTime = 0;
bool apActive = false;

// -------- PAGE HTML CONFIG WIFI --------
String htmlConfigPage() {
  return R"rawliteral(
  <!DOCTYPE html>
  <html lang="en">
  <head>
  <meta charset="utf-8">
  <title>ESP32 WiFi Setup</title>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <style>
  * { box-sizing: border-box; }
  body {
    font-family: Arial, sans-serif;
    background: #f2f2f2;
    margin: 0;
    padding: 0;
    min-height: 100vh;

    display: flex;
    flex-direction: column;  /* empile verticalement */
    justify-content: center; /* top à bottom */
    align-items: center;     /* centre horizontalement */
    padding-top: 50px;       /* espace en haut */
  }
  .ip-info {
    text-align: center;
    margin-top: 20px;        /* espace sous la card */
    font-family: Arial, sans-serif;
    font-weight: bold;
  }
  .card {
    background: #ffffff;
    padding: 20px;
    border-radius: 8px;
    width: 100%;
    max-width: 320px;
    box-shadow: 0 2px 8px rgba(0,0,0,0.15);
    text-align: center;
  }
  h2 { margin-bottom: 20px; }
  label { display: block; font-size: 14px; font-weight: bold; }
  input[type="text"], input[type="password"] {
    width: 100%; padding: 8px; margin-top:5px; margin-bottom:15px;
    border-radius: 4px; border:1px solid #ccc;
  }
  input[type="submit"] {
    width: 100%; padding: 10px; background: #007bff;
    border: none; color: white; font-size: 16px; border-radius: 4px; cursor: pointer;
  }
  </style>
  </head>
  <body>
  <div class="card">
  <h2>Configure WiFi</h2>
  <form action="/save" method="POST">
  <label for="ssid">WiFi SSID</label>
  <input type="text" id="ssid" name="ssid" required>
  <label for="password">WiFi Password</label>
  <input type="password" id="password" name="password" required>
  <input type="submit" value="Save WiFi">
  </form>
  </div>
  <div class="ip-info">
  __IP_PLACEHOLDER__
  </div>
  </body>
  </html>
)rawliteral";
}

// -------- PAGE HTML RESET WIFI --------
String htmlResetPage() {
  return R"rawliteral(
  <!DOCTYPE html>
  <html lang="en">
  <head>
  <meta charset="utf-8">
  <title>Reset WiFi</title>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <style>
  * { box-sizing: border-box; }
  body {
    font-family: Arial, sans-serif;
    background: #f2f2f2;
    margin: 0;
    padding: 0;
    min-height: 100vh;

    display: flex;
    flex-direction: column;  /* empile verticalement */
    justify-content: center; /* top à bottom */
    align-items: center;     /* centre horizontalement */
    padding-top: 50px;       /* espace en haut */
  }
  .ip-info {
    text-align: center;
    margin-top: 20px;        /* espace sous la card */
    font-family: Arial, sans-serif;
    font-weight: bold;
  }
  .card {
    background: #fff; padding: 20px;
    width: 100%; max-width: 320px; border-radius: 8px;
    box-shadow: 0 2px 8px rgba(0,0,0,0.15); text-align: center;
  }
  p { font-size: 14px; margin-bottom: 20px; }
  input[type="submit"] {
    width: 100%; padding: 10px; font-size: 16px;
    background: #dc3545; color: white; border: none; border-radius: 4px; cursor: pointer;
  }
  input[type="submit"]:hover { background: #b02a37; }
  </style>
  </head>
  <body>
  <div class="card">
  <h2>Reset WiFi</h2>
  <p>This will erase the saved WiFi settings.</p>
  <form action="/doreset" method="POST">
  <input type="submit" value="Reset WiFi">
  </form>
  </div>
  <div class="ip-info">
  __IP_PLACEHOLDER__
  </div>
  </body>
  </html>
)rawliteral";
}

// -------- HANDLERS --------
void handleRoot() {
  String page = htmlConfigPage();

  String ipText;
  if (WiFi.status() == WL_CONNECTED) {
    ipText = "Device IP: " + WiFi.localIP().toString();
  } else {
    ipText = "Device not connected to WiFi";
  }

  page.replace("__IP_PLACEHOLDER__", ipText);
  server.send(200, "text/html", page);
}

void handleSave() {
  String ssid = server.arg("ssid");
  String password = server.arg("password");

  if (ssid.length() == 0 || password.length() == 0) {
    server.send(400, "text/html", "SSID and password required");
    return;
  }

  preferences.begin("wifi", false);
  preferences.putString("ssid", ssid);
  preferences.putString("password", password);
  preferences.end();

  WiFi.begin(ssid.c_str(), password.c_str());
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 20) {
    delay(500);
    timeout++;
  }

  String ipInfo = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "Failed to connect";

  server.sendHeader("Content-Type", "text/html; charset=utf-8");
  server.send(200, "text/html",
    "<!DOCTYPE html><html lang='en'><head><meta charset='utf-8'><title>WiFi Saved</title></head>"
    "<body style='font-family: Arial, sans-serif; text-align:center; padding:20px;'>"
    "<h2>WiFi Saved Successfully</h2>"
    "<p>Device is restarting...</p>"
    "<p>IP: <b>" + ipInfo + "</b></p>"
    "</body></html>");

  // Démarrer AP temporaire pour 1 min
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(ap_ssid, ap_password);
  apStartTime = millis();
  apActive = true;

  delay(10000);
  ESP.restart();
}

void handleResetPage() {
  server.send(200, "text/html", htmlResetPage());
}

void handleDoReset() {
  preferences.begin("wifi", false);
  preferences.clear();
  preferences.end();

  server.sendHeader("Content-Type", "text/html; charset=utf-8");
  server.send(200, "text/html",
    "<!DOCTYPE html><html lang='en'><head><meta charset='utf-8'><title>Restarting</title></head>"
    "<body style='font-family: Arial, sans-serif; text-align:center; padding:20px;'>"
    "<h2>WiFi Reset Complete</h2>"
    "<p>The device is restarting...</p></body></html>");

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(ap_ssid, ap_password);
  apStartTime = millis();
  apActive = true;

  delay(2000);
  ESP.restart();
}

// -------- WEB SERVER --------
void startWebServer() {
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/reset", handleResetPage);
  server.on("/doreset", HTTP_POST, handleDoReset);
  server.begin();
}

// -------- CONNECTION WIFI --------
void connectToWiFi() {
  preferences.begin("wifi", true);
  String ssid = preferences.getString("ssid", "");
  String password = preferences.getString("password", "");
  preferences.end();

  if (ssid == "") {
    Serial.println("No WiFi saved, starting AP");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ap_ssid, ap_password);
    apStartTime = millis();
    apActive = true;
    startWebServer();
    return;
  }

  WiFi.begin(ssid.c_str(), password.c_str());
  Serial.print("Connecting to WiFi");
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 20) {
    delay(500);
    Serial.print(".");
    timeout++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected!");
    Serial.print("STA IP: "); Serial.println(WiFi.localIP());
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(ap_ssid, ap_password); // AP temporaire
    apStartTime = millis();
    apActive = true;
    startWebServer();
  } else {
    Serial.println("\nFailed to connect, starting only AP");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ap_ssid, ap_password);
    apStartTime = millis();
    apActive = true;
    startWebServer();
  }
}

// -------- SETUP / LOOP --------
void setup() {
  Serial.begin(115200);
  connectToWiFi();
}

void loop() {
  server.handleClient();

  // Désactiver AP après 1 minute
  if (apActive && millis() - apStartTime > 60000) {
    WiFi.softAPdisconnect(true);
    apActive = false;
    Serial.println("Temporary AP disabled after 1 minute");
  }
}
