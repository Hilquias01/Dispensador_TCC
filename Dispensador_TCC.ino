/************************************************************
 * Dispensador TCC – VERSÃO FINAL V5 (GPIO VERSION)
 * (Correção Erro 500 + Wi-Fi não trava + Múltiplos Horários)
 ************************************************************/

#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <AsyncJson.h>
#include <ESP8266HTTPClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <vector>

// ======================
// Definições de Hardware (USANDO GPIOs REAIS)
// ======================
// D2 = GPIO 4
#define PIN_SDA      4   
// D1 = GPIO 5
#define PIN_SCL      5   
// D5 = GPIO 14
#define PIN_BUZZER   14  
// D3 = GPIO 0 (Cuidado: deve estar HIGH no boot, o botão aterra ele)
#define PIN_BTN_STOP 0   

// LEDs dos Compartimentos (4 COMPARTIMENTOS)
// D0 = GPIO 16
#define PIN_BOX1     16  
// D6 = GPIO 12
#define PIN_BOX2     12  
// D7 = GPIO 13
#define PIN_BOX3     13  
// D8 = GPIO 15 (Cuidado: deve estar LOW no boot)
#define PIN_BOX4     15  

LiquidCrystal_I2C lcd(0x27, 16, 2);

#define AP_SSID_BASE   "Dispensador-TCC"
#define AP_PASSWORD    "12345678"
#define AP_CHANNEL     1
#define AP_MAX_CONN    4
#define TIME_OFFSET    -18000 // Fuso Acre

// ======================
// HTML na Memória Flash (Evita Erro 500)
// ======================

const char HTML_UPLOAD[] PROGMEM = R"rawliteral(
<!doctype html><html><head><meta charset='utf-8'><title>Upload</title>
<style>
body{font-family:system-ui;background:#0b1220;color:#fff;padding:20px;text-align:center}
.card{background:#0f1930;padding:20px;border-radius:16px;border:1px solid #333;max-width:400px;margin:auto}
input{width:100%;padding:12px;margin-top:10px;border-radius:10px;border:1px solid #333;background:#050910;color:white;box-sizing:border-box}
button{width:100%;padding:12px;margin-top:15px;border:0;border-radius:12px;background:#2b6bff;color:white;font-weight:bold;cursor:pointer}
</style>
</head><body>
<div class='card'>
<h2>📂 Upload LittleFS</h2>
<p style="color:#a9b6d6;font-size:12px">Use isso para atualizar o site sem cabos.</p>
<form method='POST' action='/upload' enctype='multipart/form-data'>
<input type='file' name='file'>
<button>Enviar Arquivo</button>
</form>
</div>
</body></html>
)rawliteral";

const char HTML_WIFI[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="pt-br">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width,initial-scale=1" />
  <meta name="theme-color" content="#0b1220" />
  <title>Configurar Wi-Fi</title>
  <link rel="stylesheet" href="/style.css" />
  <style>
    .center-card { max-width: 420px; margin: 0 auto; }
    .wifi-title { display: flex; justify-content: center; padding-bottom: 12px; margin-bottom: 14px; border-bottom: 1px solid #333; }
    .status-box{ background: rgba(0,0,0,.2); padding: 10px; border-radius: 10px; border: 1px dashed #444; color: #a9b6d6; font-size: 13px; text-align: center; margin-bottom: 14px; }
    .status-box strong{ color: #e9eefc; }
    .back-link{ display:block; margin-top: 14px; text-align:center; color:#666; font-size:12px; text-decoration: none; }
    input:-webkit-autofill, input:-webkit-autofill:hover, input:-webkit-autofill:focus, input:-webkit-autofill:active { -webkit-text-fill-color: #fff !important; -webkit-box-shadow: 0 0 0px 1000px #050910 inset !important; box-shadow: 0 0 0px 1000px #050910 inset !important; caret-color: #fff !important; }
  </style>
</head>
<body>
  <header class="topbar">
    <div class="brand">
      <div class="title">Configurar Wi-Fi</div>
      <div class="subtitle">Conectar o dispositivo na rede</div>
    </div>
    <div class="chip"><span class="dot"></span><span>AP</span></div>
  </header>
  <main class="wrap">
    <section class="panel">
      <div class="card center-card">
        <div class="wifi-title"><h2 style="margin:0; font-size:18px;">📡 Configurar Rede</h2></div>
        <div class="status-box">Status: <strong>%STATE%</strong></div>
        <form class="form" method="POST" action="/wifi/save">
          <label>Nome da Rede (SSID)</label>
          <input name="ssid" placeholder="Ex: MinhaCasa_2G" required autocomplete="off" />
          <label>Senha do Wi-Fi</label>
          <input name="pass" type="password" placeholder="Digite a senha aqui..." autocomplete="new-password" />
          <button class="btn" type="submit">Salvar e Conectar</button>
        </form>
        <a class="back-link" href="/">← Voltar ao Início</a>
      </div>
    </section>
  </main>
</body>
</html>
)rawliteral";


// ======================
// Objetos e Variáveis
// ======================
AsyncWebServer server(80);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", TIME_OFFSET, 60000);

struct AppConfig { String telegramUsers; bool html; };
struct WiFiCred { String ssid; String pass; };
struct Medication {
  String id; String name; String dose; String hour;
  int h, m, beforeMin; 
  int box; 
  std::vector<int> days; bool active;
};
struct NotifyResult { bool ok; int httpCode; String body; };

AppConfig cfg;
WiFiCred wifiCfg;
std::vector<Medication> meds;

bool alarmActive = false;
unsigned long lastAlarmToggle = 0;
bool ledState = false;
int lastMinuteChecked = -1;
volatile bool telegramJobPending = false;
String telegramJobMsg;
unsigned long telegramLastSendMs = 0;
File uploadFile;

// Flags para conexão Wi-Fi não-bloqueante
bool shouldConnectWifi = false;
int currentAlarmBox = 0; 

// ======================
// Utils
// ======================
String chipId() { return String(ESP.getChipId(), HEX); }

String safeString(const String& s, size_t maxLen) {
  if (s.length() <= maxLen) return s;
  return s.substring(0, maxLen);
}

String urlEncode(String s) {
  s.replace(" ", "%20"); s.replace("\n", "%0A"); return s;
}

void setBoxLeds(bool state, int boxNumber = 0) {
  if (!state) {
    digitalWrite(PIN_BOX1, LOW); digitalWrite(PIN_BOX2, LOW);
    digitalWrite(PIN_BOX3, LOW); digitalWrite(PIN_BOX4, LOW);
    return;
  }
  if (boxNumber == 1) digitalWrite(PIN_BOX1, HIGH);
  if (boxNumber == 2) digitalWrite(PIN_BOX2, HIGH);
  if (boxNumber == 3) digitalWrite(PIN_BOX3, HIGH);
  if (boxNumber == 4) digitalWrite(PIN_BOX4, HIGH);
}

void updateLCD() {
  if (alarmActive) return; 
  static unsigned long lastLcdUpdate = 0;
  if (millis() - lastLcdUpdate < 2000) return;
  lastLcdUpdate = millis();

  lcd.setCursor(0, 0);
  if (WiFi.status() == WL_CONNECTED) {
    String timeStr = timeClient.getFormattedTime();
    lcd.print("Hora: " + timeStr.substring(0, 5));
  } else {
    lcd.print("Sem Wi-Fi       ");
  }
  lcd.setCursor(0, 1);
  lcd.print("IP:" + WiFi.localIP().toString());
}

void triggerAlarm(String medName, int boxNum) {
  if (alarmActive) return;
  alarmActive = true;
  currentAlarmBox = boxNum;
  Serial.println("🚨 ALARME: " + medName + " no Compartimento " + String(boxNum));
  
  lcd.clear(); 
  lcd.setCursor(0, 0); lcd.print("PEGAR COMPARTIM.");
  lcd.setCursor(0, 1); lcd.print(String(boxNum) + ": " + safeString(medName, 13));
}

void stopAlarm() {
  if (!alarmActive) return;
  alarmActive = false;
  currentAlarmBox = 0;
  setBoxLeds(false); 
  digitalWrite(PIN_BUZZER, LOW);
  lcd.clear();
  Serial.println("🔕 Alarme parado.");
}

// ======================
// Persistência
// ======================
bool loadConfig() {
  if (!LittleFS.exists("/config.json")) return false;
  File f = LittleFS.open("/config.json", "r");
  StaticJsonDocument<512> doc; deserializeJson(doc, f); f.close();
  cfg.telegramUsers = doc["telegramUsers"] | "";
  cfg.html = doc["html"] | false;
  return true;
}

bool saveConfig() {
  StaticJsonDocument<512> doc;
  doc["telegramUsers"] = cfg.telegramUsers; doc["html"] = cfg.html;
  File f = LittleFS.open("/config.json", "w");
  serializeJson(doc, f); f.close();
  return true;
}

bool loadWiFi() {
  if (!LittleFS.exists("/wifi.json")) return false;
  File f = LittleFS.open("/wifi.json", "r");
  StaticJsonDocument<256> doc; deserializeJson(doc, f); f.close();
  wifiCfg.ssid = doc["ssid"] | ""; wifiCfg.pass = doc["pass"] | "";
  return (wifiCfg.ssid.length() > 0);
}

bool saveWiFi() {
  StaticJsonDocument<256> doc;
  doc["ssid"] = wifiCfg.ssid; doc["pass"] = wifiCfg.pass;
  File f = LittleFS.open("/wifi.json", "w"); serializeJson(doc, f); f.close();
  return true;
}

void loadMeds() {
  meds.clear();
  if (!LittleFS.exists("/meds.json")) return;
  File f = LittleFS.open("/meds.json", "r");
  DynamicJsonDocument doc(4096);
  if(deserializeJson(doc, f)) { f.close(); return; }
  f.close();
  JsonArray arr = doc.as<JsonArray>();
  for (JsonObject m : arr) {
    Medication med;
    med.id = m["id"].as<String>(); med.name = m["name"].as<String>();
    med.dose = m["dose"].as<String>(); med.hour = m["time"].as<String>();
    med.beforeMin = m["beforeMin"] | 5; med.active = m["active"] | true;
    med.box = m["box"] | 1; 

    int sep = med.hour.indexOf(':');
    if (sep > 0) { med.h = med.hour.substring(0, sep).toInt(); med.m = med.hour.substring(sep+1).toInt(); }
    for(int day : m["days"].as<JsonArray>()) med.days.push_back(day);
    meds.push_back(med);
  }
}

// ======================
// Rede (Conexão Não Bloqueante)
// ======================
void attemptWifiConnection() {
  if (wifiCfg.ssid.length() == 0) return;
  Serial.println("Tentando conectar Wi-Fi: " + wifiCfg.ssid);
  WiFi.mode(WIFI_AP_STA);
  WiFi.disconnect(); 
  delay(100);
  WiFi.begin(wifiCfg.ssid.c_str(), wifiCfg.pass.c_str());
}

NotifyResult sendTelegram(const String &text) {
  NotifyResult r; r.ok = false; r.httpCode = -1;
  if (WiFi.status() != WL_CONNECTED) return r;
  String users = cfg.telegramUsers; users.replace("@", "");
  if (users.length() == 0) return r;
  WiFiClient client; HTTPClient http; http.setTimeout(5000);
  String url = "http://api.callmebot.com/text.php?user=" + users + "&text=" + urlEncode(text) + "&html=no";
  http.begin(client, url);
  int code = http.GET(); r.httpCode = code;
  if (code > 0) { r.body = http.getString(); r.ok = (code < 400); }
  http.end(); return r;
}

// ======================
// Rotas
// ======================
void setupRoutes() {
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *req) {
    StaticJsonDocument<512> doc; doc["telegramUsers"] = cfg.telegramUsers; doc["html"] = cfg.html;
    String out; serializeJson(doc, out); req->send(200, "application/json", out);
  });
  
  AsyncCallbackJsonWebHandler *hConfig = new AsyncCallbackJsonWebHandler("/api/config", [](AsyncWebServerRequest *req, JsonVariant &json) {
    JsonObject doc = json.as<JsonObject>();
    cfg.telegramUsers = doc["telegramUsers"] | ""; cfg.html = doc["html"] | false;
    saveConfig(); req->send(200, "application/json", "{\"ok\":true}");
  });
  server.addHandler(hConfig);

  server.on("/api/meds", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (LittleFS.exists("/meds.json")) req->send(LittleFS, "/meds.json", "application/json");
    else req->send(200, "application/json", "[]");
  });
  
  AsyncCallbackJsonWebHandler *hMeds = new AsyncCallbackJsonWebHandler("/api/meds", [](AsyncWebServerRequest *req, JsonVariant &json) {
    if (!json.is<JsonArray>()) { req->send(400, "application/json", "{\"error\":\"Array\"}"); return; }
    File f = LittleFS.open("/meds.json", "w");
    if (f) { serializeJson(json, f); f.close(); loadMeds(); req->send(200, "application/json", "{\"ok\":true}"); }
    else req->send(500);
  });
  server.addHandler(hMeds);

  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *req) {
    StaticJsonDocument<512> doc;
    doc["wifi"] = (WiFi.status() == WL_CONNECTED); doc["ssid"] = WiFi.SSID();
    doc["ip"] = WiFi.localIP().toString(); doc["time"] = timeClient.getFormattedTime();
    doc["alarm"] = alarmActive;
    String out; serializeJson(doc, out); req->send(200, "application/json", out);
  });

  server.on("/api/stop-alarm", HTTP_POST, [](AsyncWebServerRequest *req) {
    stopAlarm(); req->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/test-telegram", HTTP_GET, [](AsyncWebServerRequest *req) {
    telegramJobMsg = "✅ Teste OK. Hora: " + timeClient.getFormattedTime();
    telegramJobPending = true; req->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/upload", HTTP_GET, [](AsyncWebServerRequest *req) { req->send_P(200, "text/html", HTML_UPLOAD); });
  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *req) { req->send(200, "text/plain", "OK"); }, 
    [](AsyncWebServerRequest *req, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (!index) uploadFile = LittleFS.open("/" + filename, "w");
    if (uploadFile) uploadFile.write(data, len);
    if (final && uploadFile) uploadFile.close();
  });

  server.on("/wifi", HTTP_GET, [](AsyncWebServerRequest *req) {
    String state = wifiCfg.ssid.length() ? ("Salvo: " + wifiCfg.ssid) : "Nenhum";
    auto p = [state](const String& var){ if(var=="STATE") return state; return String(); };
    req->send_P(200, "text/html", HTML_WIFI, p);
  });

  // CORREÇÃO: Salva e agenda conexão para depois (não trava)
  server.on("/wifi/save", HTTP_POST, [](AsyncWebServerRequest *req) {
    if (req->hasParam("ssid", true)) {
      wifiCfg.ssid = req->getParam("ssid", true)->value();
      wifiCfg.pass = req->getParam("pass", true)->value();
      saveWiFi(); 
      shouldConnectWifi = true; 
      req->send(200, "text/plain", "Salvo! O ESP vai tentar conectar em breve...");
    } else req->send(400);
  });
}

// ======================
// Scheduler e Loop
// ======================
void checkSchedule() {
  if (!timeClient.isTimeSet()) return;
  int currentH = timeClient.getHours();
  int currentM = timeClient.getMinutes();
  int currentWday = timeClient.getDay();
  if (currentM == lastMinuteChecked) return;
  lastMinuteChecked = currentM;

  Serial.printf("Checando: %02d:%02d\n", currentH, currentM);

  for (const auto& med : meds) {
    if (!med.active) continue;
    bool dayMatch = false;
    for (int d : med.days) if (d == currentWday) dayMatch = true;
    if (!dayMatch) continue;

    int nowMinutes = currentH * 60 + currentM;
    int medMinutes = med.h * 60 + med.m;

    if (medMinutes > nowMinutes && (medMinutes - nowMinutes) == med.beforeMin) {
      telegramJobMsg = "⚠️ PREPARAR: Em " + String(med.beforeMin) + "min tomar " + med.name + " (Compartimento " + String(med.box) + ")";
      telegramJobPending = true;
    }
    
    if (currentH == med.h && currentM == med.m) {
      telegramJobMsg = "🚨 HORA DO REMÉDIO: " + med.name + " (" + med.dose + ") -> PEGAR NO COMPARTIMENTO " + String(med.box);
      telegramJobPending = true;
      triggerAlarm(med.name, med.box);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_BTN_STOP, INPUT_PULLUP);
  pinMode(PIN_BOX1, OUTPUT); pinMode(PIN_BOX2, OUTPUT);
  pinMode(PIN_BOX3, OUTPUT); pinMode(PIN_BOX4, OUTPUT);
  
  setBoxLeds(false);
  digitalWrite(PIN_BUZZER, LOW);

  Wire.begin(PIN_SDA, PIN_SCL);
  lcd.begin(16, 2); lcd.init(); lcd.backlight();
  lcd.setCursor(0,0); lcd.print("Iniciando...");

  if(!LittleFS.begin()) Serial.println("FS Error");
  loadConfig(); loadWiFi(); loadMeds();

  String apName = AP_SSID_BASE "-" + chipId();
  WiFi.softAP(apName.c_str(), AP_PASSWORD, AP_CHANNEL, false, 4);
  
  if (wifiCfg.ssid.length() > 0) shouldConnectWifi = true;

  timeClient.begin();
  setupRoutes();
  server.begin();
  lcd.clear();
}

void loop() {
  timeClient.update();
  checkSchedule();

  if (shouldConnectWifi) {
    shouldConnectWifi = false;
    attemptWifiConnection();
  }

  if (telegramJobPending) {
    if (millis() - telegramLastSendMs > 2000) {
      telegramJobPending = false; telegramLastSendMs = millis();
      sendTelegram(telegramJobMsg);
    }
  }

  if (alarmActive) {
    if (digitalRead(PIN_BTN_STOP) == LOW) { 
      stopAlarm(); delay(500); 
    } else {
      if (millis() - lastAlarmToggle > 500) {
        lastAlarmToggle = millis(); ledState = !ledState;
        if (ledState) {
          setBoxLeds(true, currentAlarmBox); tone(PIN_BUZZER, 1000, 200);
        } else {
          setBoxLeds(false);
        }
      }
    }
  } 
  updateLCD();
  delay(10);
}
