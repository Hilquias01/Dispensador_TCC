#include <ESP8266WiFi.h>
#include <WiFiManager.h>

#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include <LittleFS.h>
#include <ArduinoJson.h>

#include <ESP8266HTTPClient.h>

// ======================
// Configuração e servidor
// ======================
AsyncWebServer server(80);

struct AppConfig {
  String whatsappPhone; // Ex: 5568999999999
  String callmebotKey;  // Ex: 1234567
};

AppConfig cfg;

// ======================
// Utils
// ======================
String chipId() {
  return String(ESP.getChipId(), HEX);
}

String safeString(const String& s, size_t maxLen) {
  if (s.length() <= maxLen) return s;
  return s.substring(0, maxLen);
}

String urlEncodeBasic(String s) {
  // Simples e suficiente para mensagens curtas
  s.replace("%", "%25");
  s.replace(" ", "%20");
  s.replace("\n", "%0A");
  s.replace("\r", "");
  s.replace("&", "%26");
  s.replace("?", "%3F");
  s.replace("=", "%3D");
  s.replace("#", "%23");
  return s;
}

// ======================
// Persistência (LittleFS)
// ======================
bool loadConfig() {
  if (!LittleFS.exists("/config.json")) return false;

  File f = LittleFS.open("/config.json", "r");
  if (!f) return false;

  StaticJsonDocument<384> doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return false;

  cfg.whatsappPhone = String((const char*) (doc["whatsappPhone"] | ""));
  cfg.callmebotKey  = String((const char*) (doc["callmebotKey"]  | ""));

  cfg.whatsappPhone = safeString(cfg.whatsappPhone, 20);
  cfg.callmebotKey  = safeString(cfg.callmebotKey, 32);

  return true;
}

bool saveConfig() {
  StaticJsonDocument<384> doc;
  doc["whatsappPhone"] = cfg.whatsappPhone;
  doc["callmebotKey"]  = cfg.callmebotKey;

  File f = LittleFS.open("/config.json", "w");
  if (!f) return false;

  serializeJson(doc, f);
  f.close();
  return true;
}

// ======================
// WhatsApp (CallMeBot)
// ======================
bool sendWhatsApp(const String& message) {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (cfg.whatsappPhone.length() < 10) return false;
  if (cfg.callmebotKey.length() < 5) return false;

  WiFiClient client;
  HTTPClient http;

  String msg = urlEncodeBasic(message);

  // CallMeBot: geralmente funciona via HTTP
  String url = "http://api.callmebot.com/whatsapp.php?phone=" + cfg.whatsappPhone +
               "&text=" + msg +
               "&apikey=" + cfg.callmebotKey;

  if (!http.begin(client, url)) return false;

  int code = http.GET();
  http.end();

  return (code > 0 && code < 400);
}

// ======================
// Rotas do servidor
// ======================
void setupRoutes() {
  // Arquivos estáticos do LittleFS
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  // Status do dispositivo
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *req){
    StaticJsonDocument<384> doc;
    doc["device"] = "Dispensador TCC";
    doc["chip_id"] = chipId();
    doc["wifi"] = (WiFi.status() == WL_CONNECTED) ? "conectado" : "desconectado";
    doc["ssid"] = WiFi.SSID();
    doc["ip"] = WiFi.localIP().toString();
    doc["rssi"] = WiFi.RSSI();

    String out;
    serializeJson(doc, out);
    req->send(200, "application/json", out);
  });

  // Retornar config (sem expor a key completa)
  server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *req){
    StaticJsonDocument<256> doc;
    doc["whatsappPhone"] = cfg.whatsappPhone;

    // máscara da key pra não aparecer inteira no painel
    String masked = cfg.callmebotKey;
    if (masked.length() > 4) {
      masked = String("****") + masked.substring(masked.length() - 2);
    } else if (masked.length() > 0) {
      masked = "****";
    }
    doc["callmebotKeyMasked"] = masked;

    String out;
    serializeJson(doc, out);
    req->send(200, "application/json", out);
  });

  // Salvar config
  server.on("/api/config", HTTP_POST, [](AsyncWebServerRequest *req){
    // Espera JSON no body
    if (!req->hasParam("plain", true)) {
      req->send(400, "application/json", "{\"ok\":false,\"error\":\"body vazio\"}");
      return;
    }

    String body = req->getParam("plain", true)->value();
    StaticJsonDocument<384> doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
      req->send(400, "application/json", "{\"ok\":false,\"error\":\"json inválido\"}");
      return;
    }

    String phone = String((const char*)(doc["whatsappPhone"] | ""));
    String key   = String((const char*)(doc["callmebotKey"] | ""));

    phone = safeString(phone, 20);
    key   = safeString(key, 32);

    // validação simples
    if (phone.length() < 10 || key.length() < 5) {
      req->send(400, "application/json", "{\"ok\":false,\"error\":\"telefone/key inválidos\"}");
      return;
    }

    cfg.whatsappPhone = phone;
    cfg.callmebotKey  = key;

    bool ok = saveConfig();
    req->send(ok ? 200 : 500, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
  });

  // Testar WhatsApp
  server.on("/api/test-whatsapp", HTTP_GET, [](AsyncWebServerRequest *req){
    bool sent = sendWhatsApp("✅ Teste OK - Dispensador TCC (ESP8266) \nIP: " + WiFi.localIP().toString());
    req->send(200, "application/json", sent ? "{\"ok\":true}" : "{\"ok\":false}");
  });
}

void setup() {
  Serial.begin(115200);
  delay(300);

  // LittleFS
  if (!LittleFS.begin()) {
    Serial.println("❌ Falha ao montar LittleFS");
  } else {
    Serial.println("✅ LittleFS montado");
  }

  loadConfig();

  // Portal Wi-Fi
  WiFiManager wm;
  wm.setConfigPortalTimeout(180); // 3 min
  wm.setWiFiAutoReconnect(true);

  // Nome do AP com chipId (fica mais profissional)
  String apName = "Dispensador-TCC-" + chipId();

  bool ok = wm.autoConnect(apName.c_str());
  if (!ok) {
    Serial.println("❌ Falha Wi-Fi. Reiniciando...");
    ESP.restart();
  }

  Serial.print("✅ Conectado! IP: ");
  Serial.println(WiFi.localIP());

  setupRoutes();
  server.begin();
  Serial.println("✅ Servidor web iniciado");
}

void loop() {
  // Parte 2: RTC + scheduler + buzzer + LEDs + envio no horário
}