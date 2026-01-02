#include <WiFi.h>
#include <PubSubClient.h>
#include "esp_camera.h"
#include "esp_sleep.h"
#include <WebServer.h>
#include <Preferences.h>

// ===================================================
// GPIO
// ===================================================
#define TRIGGER_PIN 13
#define OUTPUT_PIN  4
#define BATTERY_PIN 38   // ADC1
#define BAT_HOLD_PIN 33

// ===================================================
// PARAMÈTRES
// ===================================================
#define EDGE_COOLDOWN_MS 5000
#define MAX_CAPTURES_PER_DAY 5
#define DAY_US (24ULL * 60ULL * 60ULL * 1000000ULL)
#define CHUNK_SIZE 4000
#define WIFI_CONFIG_TIME_MS 30000  // 3 minutes

// ===================================================
// RTC DATA
// ===================================================
RTC_DATA_ATTR bool firstBoot = false;
RTC_DATA_ATTR uint32_t dailyCaptureCount = 0;
RTC_DATA_ATTR uint64_t dayStartUs = 0;

// ===================================================
// MQTT
// ===================================================
const char* mqtt_server = "172.20.10.3";
const int   mqtt_port   = 1883;

const char* mqtt_image_topic   = "esp32/image";
const char* mqtt_battery_topic = "esp32/batterie";

// ===================================================
// AP et serveur Web
// ===================================================
WiFiClient espClient;
PubSubClient client(espClient);
WebServer server(80);
Preferences preferences;

const char* ap_ssid = "ESP32_Config";
const char* ap_password = "12345678";
unsigned long apStartTime = 0;
bool apActive = false;

// ===================================================
// Prototypes
// ===================================================
String htmlConfigPage();
String htmlResetPage();
void handleRoot();
void handleSave();
void handleResetPage();
void handleDoReset();
void startWebServer();
void connectToWiFi();
bool setupCamera();
bool reconnectMQTT();
float readBatteryVoltage();
void publishBatteryVoltage();
void publishImage(camera_fb_t* fb);

// ===================================================
// SETUP
// ===================================================
void setup() {
    Serial.begin(115200);
    delay(200);

    pinMode(BAT_HOLD_PIN, OUTPUT);
    digitalWrite(BAT_HOLD_PIN, HIGH);

    pinMode(TRIGGER_PIN, INPUT);
    pinMode(OUTPUT_PIN, OUTPUT);
    digitalWrite(OUTPUT_PIN, LOW);

    uint64_t nowUs = esp_timer_get_time();

    if (!firstBoot) {
        Serial.println("[SYS] Premier boot");
        dayStartUs = nowUs;
        dailyCaptureCount = 0;
        firstBoot = true;
    }

    if (nowUs - dayStartUs >= DAY_US) {
        Serial.println("[SYS] Nouvelle journée");
        dayStartUs = nowUs;
        dailyCaptureCount = 0;
    }

    if (dailyCaptureCount >= MAX_CAPTURES_PER_DAY) {
        Serial.println("[SYS] Limite journalière atteinte");
        esp_sleep_enable_timer_wakeup(DAY_US - (nowUs - dayStartUs));
        esp_deep_sleep_start();
    }

    // --- Wakeup cause ---
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    Serial.printf("[WAKE] Cause=%d\n", cause);

if (cause == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println("[CAPTURE] Déclenchement par trigger");
    dailyCaptureCount++;
    digitalWrite(OUTPUT_PIN, HIGH);

    bool captureOK = true;
    camera_fb_t* fb = nullptr;

    if (!setupCamera()) captureOK = false;

    if (captureOK) {
        connectToWiFi();
        if (WiFi.status() != WL_CONNECTED) captureOK = false;
    }

    if (captureOK && !reconnectMQTT()) captureOK = false;

    if (captureOK) {
        fb = esp_camera_fb_get();
        if (!fb) captureOK = false;
    }

    if (captureOK) {
        publishImage(fb);
        delay(100);
        publishBatteryVoltage();
        esp_camera_fb_return(fb);
        Serial.println("[CAPTURE] Image + batterie envoyées");
    }

    digitalWrite(OUTPUT_PIN, LOW);

    Serial.println("[SLEEP] Cooldown");
    esp_sleep_enable_timer_wakeup(EDGE_COOLDOWN_MS * 1000ULL);
    esp_deep_sleep_start();

    return;  // ⬅️⬅️⬅️ LIGNE CRITIQUE
}


// --- Boot NORMAL uniquement ---
if (cause == ESP_SLEEP_WAKEUP_UNDEFINED) {

    preferences.begin("wifi", true);
    String ssid = preferences.getString("ssid", "");
    String password = preferences.getString("password", "");
    preferences.end();

    if (ssid == "") {
        Serial.println("[CONFIG] Aucun WiFi configuré, mode AP actif");
        WiFi.mode(WIFI_AP);
        WiFi.softAP(ap_ssid, ap_password);
        startWebServer();

        unsigned long start = millis();
        while (millis() - start < WIFI_CONFIG_TIME_MS) {
            server.handleClient();
        }

        WiFi.softAPdisconnect(true);
        Serial.println("[AP] Temps écoulé");
    } else {
        Serial.print("[WIFI] Tentative de connexion à ");
        Serial.println(ssid);

        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid.c_str(), password.c_str());

        int timeout = 0;
        while (WiFi.status() != WL_CONNECTED && timeout < 20) {
            delay(500);
            timeout++;
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("[WIFI] Connecté");
            startWebServer();
        }

        unsigned long start = millis();
        while (millis() - start < WIFI_CONFIG_TIME_MS) {
            server.handleClient();
            delay(100);
        }
    }
}


    // --- Après attente, mettre ESP en attente trigger ---
    Serial.println("[SLEEP] Attente front montant");
    esp_sleep_enable_ext0_wakeup((gpio_num_t)TRIGGER_PIN, 1);
    esp_deep_sleep_start();
}

void loop() {
    if (apActive) server.handleClient();
}

// ===================================================
// HTML
// ===================================================

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

// ===================================================
// CAMERA
// ===================================================
bool setupCamera() {
    Serial.println("[CAM] Initialisation caméra");
 
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer   = LEDC_TIMER_0;
 
    config.pin_d0 = 32; config.pin_d1 = 35;
    config.pin_d2 = 34; config.pin_d3 = 5;
    config.pin_d4 = 39; config.pin_d5 = 18;
    config.pin_d6 = 36; config.pin_d7 = 19;
 
    config.pin_xclk  = 27;
    config.pin_pclk  = 21;
    config.pin_vsync = 22;
    config.pin_href  = 26;
    config.pin_sscb_sda = 25;
    config.pin_sscb_scl = 23;
    config.pin_pwdn  = -1;
    config.pin_reset = 15;
 
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size   = FRAMESIZE_SXGA;
    config.jpeg_quality = 5;
    config.fb_count     = 1;
 
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("[CAM][ERREUR] Init failed 0x%x\n", err);
        return false;
    }
 
    Serial.println("[CAM] OK");
    return true;
}

 
// ===================================================
// MQTT
// ===================================================
bool reconnectMQTT() {
    Serial.println("[MQTT] Connexion broker");
    client.setServer(mqtt_server, mqtt_port);
    client.setBufferSize(4096);
 
    unsigned long start = millis();
    while (!client.connected()) {
        if (client.connect("TimerCamESP32")) {
            Serial.println("[MQTT] Connecté");
            return true;
        }
        delay(1000);
        if (millis() - start > 10000) {
            Serial.println("[MQTT][ERREUR] Timeout");
            return false;
        }
    }
    return true;
}
 
// ===================================================
// BATTERIE
// ===================================================
float readBatteryVoltage() {
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
 
    int raw = analogRead(BATTERY_PIN);
    float v_adc = (raw / 4095.0f) * 3.3f;  // tension mesurée par l'ADC
    Serial.printf("batterie raw = %d \n", raw);
 
    // Facteur correctif pour le pont diviseur (diviseur 0.66)
    const float FACTOR = 1.515f;  // 1 / 0.66
    float v_bat = v_adc * FACTOR;
 
    Serial.printf("[BAT] raw=%d  Vbat=%.2fV\n", raw, v_bat);
    return v_bat;
}
 
void publishBatteryVoltage() {
    float vbat = readBatteryVoltage();
 
    char payload[16];
    snprintf(payload, sizeof(payload), "%.2f", vbat);
 
    bool ok = client.publish(mqtt_battery_topic, payload, false);
    Serial.printf("[MQTT] Batterie %.2f V → %s\n",
                  vbat, ok ? "OK" : "FAIL");
}
 
// ===================================================
// IMAGE MQTT
// ===================================================
void publishImage(camera_fb_t* fb) {
    Serial.printf("[MQTT] Image %u octets\n", fb->len);
 
    uint32_t totalChunks = (fb->len + CHUNK_SIZE - 1) / CHUNK_SIZE;
    Serial.printf("[MQTT] %lu chunks\n", totalChunks);
 
    for (uint32_t i = 0; i < totalChunks; i++) {
        uint32_t offset = i * CHUNK_SIZE;
        uint32_t size   = min((uint32_t)CHUNK_SIZE, fb->len - offset);
 
        uint8_t* payload = (uint8_t*)malloc(size + 4);
        if (!payload) {
            Serial.println("[MQTT] malloc failed");
            return;
        }
 
        payload[0] = i >> 8;
        payload[1] = i & 0xFF;
        payload[2] = totalChunks >> 8;
        payload[3] = totalChunks & 0xFF;
 
        memcpy(payload + 4, fb->buf + offset, size);
 
        bool ok = client.publish(mqtt_image_topic, payload, size + 4, false);
        Serial.printf("[MQTT] Chunk %lu/%lu (%lu bytes) → %s\n",
                      i + 1, totalChunks, size, ok ? "OK" : "FAIL");
 
        free(payload);
        client.loop();
        delay(20);
    }
}