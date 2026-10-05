// Live AQI Monitor: ESP32 + Sharp GP2Y1010AU0F, reporting to ThingsBoard over MQTT
// and scrolling the AQI on a P10 LED panel.

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "config.h"
#include "secrets.h"
#include "dust_sensor.h"
#include "aqi.h"
#include "display.h"

static const char* kTopicTelemetry  = "v1/devices/me/telemetry";
static const char* kTopicAttributes = "v1/devices/me/attributes";
static const char* kTopicAttrReq    = "v1/devices/me/attributes/request/1";
static const char* kTopicAttrResp   = "v1/devices/me/attributes/response/+";

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
DustSensor dust;
Display display;

uint32_t telemetryIntervalMs = TELEMETRY_INTERVAL_S_DEFAULT * 1000UL;
uint32_t lastTelemetryMs = 0;
uint32_t lastMqttAttemptMs = 0;

// Ring buffer of recent window densities for the rolling AQI average.
float history[AQI_AVG_WINDOWS];
size_t historyCount = 0, historyHead = 0;

float rollingAverage(float latest) {
  history[historyHead] = latest;
  historyHead = (historyHead + 1) % AQI_AVG_WINDOWS;
  if (historyCount < AQI_AVG_WINDOWS) historyCount++;
  float sum = 0;
  for (size_t i = 0; i < historyCount; i++) sum += history[i];
  return sum / historyCount;
}

// Shared attributes from ThingsBoard: either the response to our request
// ({"shared": {...}}) or a pushed update ({...}).
void applySharedAttributes(JsonObjectConst attrs) {
  if (attrs["telemetryIntervalSec"].is<int>()) {
    int s = attrs["telemetryIntervalSec"];
    if (s >= 2 && s <= 3600) {
      telemetryIntervalMs = s * 1000UL;
      Serial.printf("[cfg] telemetry interval = %d s\n", s);
    }
  }
  if (attrs["dustZeroVoltage"].is<float>()) {
    float v = attrs["dustZeroVoltage"];
    if (v > 0.0f && v < 3.0f) {
      dust.setZeroVoltage(v);
      Serial.printf("[cfg] dust zero voltage = %.3f V\n", v);
    }
  }
}

void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  JsonDocument doc;
  if (deserializeJson(doc, payload, length)) return;
  JsonObjectConst root = doc.as<JsonObjectConst>();
  if (root["shared"].is<JsonObjectConst>()) {
    applySharedAttributes(root["shared"]);
  } else {
    applySharedAttributes(root);
  }
}

// Non-blocking so the display and sensor run while Wi-Fi comes up;
// the ESP32 keeps reconnecting in the background.
void startWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("[wifi] connecting to %s\n", WIFI_SSID);
}

void logWifiChanges() {
  static bool wasConnected = false;
  bool connected = WiFi.status() == WL_CONNECTED;
  if (connected == wasConnected) return;
  wasConnected = connected;
  if (connected) {
    Serial.printf("[wifi] connected, IP %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("[wifi] disconnected, retrying");
  }
}

void publishDeviceAttributes() {
  JsonDocument doc;
  doc["firmwareVersion"] = FIRMWARE_VERSION;
  doc["sensor"] = "Sharp GP2Y1010AU0F";
  doc["board"] = "ESP32";
  doc["ip"] = WiFi.localIP().toString();
  doc["mac"] = WiFi.macAddress();
  char buf[256];
  size_t n = serializeJson(doc, buf);
  mqtt.publish(kTopicAttributes, (const uint8_t*)buf, n, false);
}

// Non-blocking: tries at most once every 5 s so sensor sampling keeps running.
void maintainMqtt() {
  if (mqtt.connected()) return;
  if (WiFi.status() != WL_CONNECTED) return;
  if (millis() - lastMqttAttemptMs < 5000) return;
  lastMqttAttemptMs = millis();

  String clientId = "aqi-" + WiFi.macAddress();
  Serial.printf("[mqtt] connecting to %s:%d ... ", TB_HOST, TB_PORT);
  // ThingsBoard authenticates devices with the access token as the username.
  if (mqtt.connect(clientId.c_str(), TB_ACCESS_TOKEN, nullptr)) {
    Serial.println("ok");
    mqtt.subscribe(kTopicAttributes);
    mqtt.subscribe(kTopicAttrResp);
    mqtt.publish(kTopicAttrReq, "{\"sharedKeys\":\"telemetryIntervalSec,dustZeroVoltage\"}");
    publishDeviceAttributes();
  } else {
    // 5 = not authorized: wrong access token.
    Serial.printf("failed, state=%d\n", mqtt.state());
  }
}

void publishTelemetry() {
  DustSensor::Window w = dust.takeWindow();
  if (w.samples == 0) return;

  float pm25Avg = rollingAverage(w.density);
  int aqi = aqiFromPm25(pm25Avg);

  JsonDocument doc;
  doc["dustVoltage"] = serialized(String(w.voltage, 3));
  doc["dustZeroVoltage"] = serialized(String(dust.zeroVoltage(), 3));
  doc["pm25"] = serialized(String(w.density, 1));
  doc["pm25Avg"] = serialized(String(pm25Avg, 1));
  doc["aqi"] = aqi;
  doc["aqiCategory"] = aqiCategory(aqi);
  doc["samples"] = w.samples;
  doc["rssi"] = WiFi.RSSI();

  char buf[384];
  size_t n = serializeJson(doc, buf);
  Serial.printf("[data] %s\n", buf);
  if (mqtt.connected()) mqtt.publish(kTopicTelemetry, (const uint8_t*)buf, n, false);

  char msg[64];
  snprintf(msg, sizeof(msg), "AQI: %d  %s", aqi, aqiCategory(aqi));
  display.show(msg);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.printf("\nLive AQI Monitor v%s\n", FIRMWARE_VERSION);

  display.begin();
  display.show("Waiting for sensor...");
  dust.begin();
  startWifi();

  mqtt.setServer(TB_HOST, TB_PORT);
  mqtt.setBufferSize(512);
  mqtt.setCallback(onMqttMessage);

  lastTelemetryMs = millis();
}

void loop() {
  display.update();
  logWifiChanges();
  maintainMqtt();
  mqtt.loop();

  if (millis() - lastTelemetryMs >= telemetryIntervalMs) {
    lastTelemetryMs = millis();
    publishTelemetry();
  }
}
