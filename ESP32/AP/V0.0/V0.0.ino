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
    <html>
    <head>
      <meta charset="utf-8">
      <title>Configuration WiFi ESP32</title>
    </head>
    <body>
      <h2>Configurer le WiFi</h2>
      <form action="/save" method="POST">
        SSID:<br>
        <input type="text" name="ssid"><br><br>
        Mot de passe:<br>
        <input type="password" name="password"><br><br>
        <input type="submit" value="Enregistrer">
      </form>
    </body>
    </html>
  )rawliteral";
}

// -------- PAGE HTML RESET WIFI --------
String htmlResetPage() {
  return R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
      <meta charset="utf-8">
      <title>Reset WiFi</title>
    </head>
    <body>
      <h2>Reconfigurer le WiFi</h2>
      <p>Cette action effacera le WiFi enregistré.</p>
      <form action="/doreset" method="POST">
        <input type="submit" value="Réinitialiser le WiFi">
      </form>
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

  server.send(200, "text/html",
    "<h2>WiFi enregistré</h2><p>Redémarrage...</p>");

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

  server.send(200, "text/html",
    "<h2>WiFi effacé</h2><p>Redémarrage...</p>");

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
    startWebServer();   // 👈 PAGE RESET ACTIVE
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
