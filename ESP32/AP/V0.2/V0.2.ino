#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

WebServer server(80);
Preferences preferences;

// Nom du point d'accès ESP32
const char* ap_ssid = "ESP32_Config";
const char* ap_password = "12345678";

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
        * {
          box-sizing: border-box;
        }
      
        body {
          font-family: Arial, sans-serif;
          background: #f2f2f2;
          margin: 0;
          padding: 0;
          min-height: 100vh;
      
          display: flex;
          justify-content: center;
          align-items: center;
        }
      
        .card {
          background: #ffffff;
          padding: 20px;
          border-radius: 8px;
          width: 100%;
          max-width: 320px;
          box-shadow: 0 2px 8px rgba(0,0,0,0.15);
        }
      
        h2 {
          text-align: center;
          margin-bottom: 20px;
        }
      
        form {
          width: 100%;
        }
      
        label {
          display: block;
          font-size: 14px;
          font-weight: bold;
        }
      
        input[type="text"],
        input[type="password"] {
          width: 100%;
          padding: 8px;
          margin-top: 5px;
          margin-bottom: 15px;
          border-radius: 4px;
          border: 1px solid #ccc;
          display: block;
        }
      
        input[type="submit"] {
          width: 100%;
          padding: 10px;
          background: #007bff;
          border: none;
          color: white;
          font-size: 16px;
          border-radius: 4px;
          cursor: pointer;
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
        min-height: 100vh;
        display: flex;
        justify-content: center;
        align-items: center;
      }

      .card {
        background: #fff;
        padding: 20px;
        width: 100%;
        max-width: 320px;
        border-radius: 8px;
        box-shadow: 0 2px 8px rgba(0,0,0,0.15);
        text-align: center;
      }

      p {
        font-size: 14px;
        margin-bottom: 20px;
      }

      input[type="submit"] {
        width: 100%;
        padding: 10px;
        font-size: 16px;
        background: #dc3545;
        color: white;
        border: none;
        border-radius: 4px;
        cursor: pointer;
      }

      input[type="submit"]:hover {
        background: #b02a37;
      }
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
  </body>
  </html>
  )rawliteral";
}

// -------- ROUTES WEB --------
void handleRoot() {
  server.send(200, "text/html", htmlConfigPage());
}

void handleSave() {
  String ssid = server.arg("ssid");
  String password = server.arg("password");

  preferences.begin("wifi", false);
  preferences.putString("ssid", ssid);
  preferences.putString("password", password);
  preferences.end();

  server.sendHeader("Content-Type", "text/html; charset=utf-8");
  server.send(200, "text/html",
    "<!DOCTYPE html>"
    "<html lang='en'>"
    "<head><meta charset='utf-8'><title>WiFi Saved</title></head>"
    "<body style='font-family: Arial, sans-serif; text-align:center; padding:20px;'>"
    "<h2>WiFi Saved Successfully</h2>"
    "<p>The device is restarting. Please reconnect in a few seconds.</p>"
    "</body></html>");

    delay(2000);
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
    "<!DOCTYPE html>"
    "<html lang='en'>"
    "<head><meta charset='utf-8'><title>Restarting</title></head>"
    "<body style='font-family: Arial, sans-serif; text-align:center; padding:20px;'>"
    "<h2>WiFi Reset Complete</h2>"
    "<p>The device is restarting. Please reconnect in a few seconds.</p>"
    "</body></html>");

  delay(2000);
  ESP.restart();
}

// -------- MODES WIFI --------
void startAPMode() {
  WiFi.mode(WIFI_AP);  
  WiFi.softAP(ap_ssid, ap_password);

  Serial.println("Mode AP actif");
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
}

void startWebServer() {
  server.on("/reset", handleResetPage);
  server.on("/doreset", HTTP_POST, handleDoReset);
  server.begin();
}

void connectToWiFi() {
  preferences.begin("wifi", true);
  String ssid = preferences.getString("ssid", "");
  String password = preferences.getString("password", "");
  preferences.end();

  if (ssid == "") {
    Serial.println("Aucun WiFi enregistré");
    startAPMode();
    return;
  }

  WiFi.begin(ssid.c_str(), password.c_str());
  Serial.print("Connexion au WiFi");

  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 20) {
    delay(500);
    Serial.print(".");
    timeout++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnecté !");
    Serial.println(WiFi.localIP());
    WiFi.mode(WIFI_STA);   
    startWebServer();
  } else {
    Serial.println("\nÉchec connexion");
    startAPMode();
  }
}

// -------- SETUP / LOOP --------
void setup() {
  Serial.begin(115200);
  connectToWiFi();
}

void loop() {
  server.handleClient();
}
