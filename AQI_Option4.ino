#include <Wire.h>
#include <DFRobot_HX711_I2C.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// --- Wi-Fi + ThingsBoard (fill in, don't commit real values) ---
#define WIFI_SSID       "your-wifi-ssid"
#define WIFI_PASSWORD   "your-wifi-password"
#define TB_HOST         "thingsboard.cloud"
#define TB_PORT         1883
#define TB_ACCESS_TOKEN "YOUR_DEVICE_ACCESS_TOKEN"

static const char* kTopicTelemetry = "v1/devices/me/telemetry";
const unsigned long TELEMETRY_INTERVAL_MS = 1000;

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
unsigned long lastMqttAttemptMs = 0;
unsigned long lastTelemetryMs = 0;

// --- I2C Weight Sensor ---
DFRobot_HX711_I2C scale(&Wire, 0x64); 

// --- Pin Definitions ---
#define SDA_PIN 8
#define SCL_PIN 9
const int START_BUTTON_PIN = 6; 
const int TARE_BUTTON_PIN = 36; 
const int PUMP_PWM_PIN = 18; 

// --- Filling Parameters ---
const float TARGET_WEIGHT = 300.0; // Set your final desired total weight here
const float PRE_ACT_OFFSET = 5.0;   // The water still in the air when pump cuts off
const float SHUTOFF_POINT = TARGET_WEIGHT - PRE_ACT_OFFSET;

// --- Noise Filter ---
const float ZERO_TOLERANCE = 2.0; // Any weight between -2.0g and 2.0g is forced to 0.0g

// --- Speed Thresholds ---
const float STAGE_1_WEIGHT = TARGET_WEIGHT * 0.80; 
const float STAGE_2_WEIGHT = TARGET_WEIGHT * 0.92; 

// --- Motor Speed Limits (0-255) ---
const int MAX_PWM = 255;  
const int MID_PWM = 160;  
const int MIN_PWM = 90;   

bool isFilling = false;
float lastWeight = 0.0;
int lastPwm = 0;
String lastStatus = "Idle";
unsigned long lastSerialPrintTime = 0; 

// --- Debounce Variables ---
bool lastStartBtnState = HIGH;
unsigned long lastStartDebounceTime = 0;

bool lastTareBtnState = HIGH;
unsigned long lastTareDebounceTime = 0;

void setup() {
  Serial.begin(115200);
  
  pinMode(PUMP_PWM_PIN, OUTPUT);
  analogWrite(PUMP_PWM_PIN, 0); 
  
  pinMode(START_BUTTON_PIN, INPUT_PULLUP); 
  pinMode(TARE_BUTTON_PIN, INPUT_PULLUP);

  Wire.begin(SDA_PIN, SCL_PIN);
  
  Serial.println("Initializing Weight Sensor...");
  while (!scale.begin()) {
    Serial.println("Failed to find I2C Weight Sensor. Check wiring!");
    delay(1000);
  }
  
  // Applied the tested calibration value
  scale.setCalibration(2005.0); 
  
  scale.peel(); 

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("[wifi] connecting to %s
", WIFI_SSID);
  mqtt.setServer(TB_HOST, TB_PORT);
  mqtt.setBufferSize(512);

  Serial.println("System Ready!");
}

// Non-blocking: retries the MQTT connection at most every 5 s.
void maintainMqtt() {
  if (mqtt.connected()) return;
  if (WiFi.status() != WL_CONNECTED) return;
  if (millis() - lastMqttAttemptMs < 5000) return;
  lastMqttAttemptMs = millis();

  String clientId = "filler-" + WiFi.macAddress();
  Serial.printf("[mqtt] connecting to %s:%d ... ", TB_HOST, TB_PORT);
  // ThingsBoard authenticates devices with the access token as the username.
  if (mqtt.connect(clientId.c_str(), TB_ACCESS_TOKEN, nullptr)) {
    Serial.println("ok");
  } else {
    Serial.printf("failed, state=%d
", mqtt.state());
  }
}

void publishTelemetry(const char* event) {
  if (!mqtt.connected()) return;
  JsonDocument doc;
  doc["weight"] = lastWeight;
  doc["targetWeight"] = TARGET_WEIGHT;
  doc["pwm"] = lastPwm;
  doc["status"] = lastStatus;
  doc["isFilling"] = isFilling;
  doc["rssi"] = WiFi.RSSI();
  if (event) doc["event"] = event;
  char buf[256];
  size_t n = serializeJson(doc, buf, sizeof(buf));
  mqtt.publish(kTopicTelemetry, (const uint8_t*)buf, n, false);
}

void loop() {
  unsigned long currentMillis = millis();

  maintainMqtt();
  mqtt.loop();
  if (currentMillis - lastTelemetryMs >= TELEMETRY_INTERVAL_MS) {
    lastTelemetryMs = currentMillis;
    publishTelemetry(nullptr);
  }

  // ==========================================
  // 1. TARE BUTTON LOGIC (Manual Zero)
  // ==========================================
  bool currentTareBtnState = digitalRead(TARE_BUTTON_PIN);
  if (currentTareBtnState != lastTareBtnState) {
    lastTareDebounceTime = currentMillis; 
  }
  
  if ((currentMillis - lastTareDebounceTime) > 50) {
    if (currentTareBtnState == LOW && !isFilling) {
      scale.peel(); 
      Serial.println("\n*** SCALE ZEROED MANUALLY ***");
      delay(200); 
    }
  }
  lastTareBtnState = currentTareBtnState;

  // ==========================================
  // 2. START BUTTON LOGIC
  // ==========================================
  bool currentStartBtnState = digitalRead(START_BUTTON_PIN);
  if (currentStartBtnState != lastStartBtnState) {
    lastStartDebounceTime = currentMillis; 
  }
  
  if ((currentMillis - lastStartDebounceTime) > 50) {
    if (currentStartBtnState == LOW && !isFilling) {
      
      float check_weight = scale.readWeight(3);
      if (check_weight > -ZERO_TOLERANCE && check_weight < ZERO_TOLERANCE) {
        check_weight = 0.0; 
      }
      
      if (check_weight < SHUTOFF_POINT) {
        isFilling = true;
        Serial.println("\n--- FILL CYCLE STARTED ---");
      } else {
        Serial.println("\nWARNING: Target weight already reached! Cannot start.");
        delay(500); 
      }
    }
  }
  lastStartBtnState = currentStartBtnState; 

  // ==========================================
  // 3. ACTIVE FILLING LOGIC
  // ==========================================
  if (isFilling) {
    float current_weight = scale.readWeight(3); 
    
    // Apply Deadband Filter near zero
    if (current_weight > -ZERO_TOLERANCE && current_weight < ZERO_TOLERANCE) {
      current_weight = 0.0;
    }

    int pwmSpeed = 0;
    String currentStage = "";

    if (current_weight < STAGE_1_WEIGHT) {
      pwmSpeed = MAX_PWM;
      currentStage = "Bulk Fill";
    }
    else if (current_weight >= STAGE_1_WEIGHT && current_weight < STAGE_2_WEIGHT) {
      pwmSpeed = MID_PWM;
      currentStage = "Med Fill";
    }
    else if (current_weight >= STAGE_2_WEIGHT && current_weight < SHUTOFF_POINT) {
      pwmSpeed = MIN_PWM;
      currentStage = "Dribble ";
    }
    else if (current_weight >= SHUTOFF_POINT) {
      pwmSpeed = 0;
      isFilling = false; 
      
      analogWrite(PUMP_PWM_PIN, 0); 
      printToSerial(current_weight, 0, "Settling...", true);
      publishTelemetry("fillComplete");
      
      delay(1500); 
      return; 
    }

    analogWrite(PUMP_PWM_PIN, pwmSpeed);
    printToSerial(current_weight, pwmSpeed, currentStage, false);
  }
  
  // ==========================================
  // 4. IDLE LOGIC
  // ==========================================
  else {
    float idle_weight = scale.readWeight(3);
    
    // Apply Deadband Filter to idle display
    if (idle_weight > -ZERO_TOLERANCE && idle_weight < ZERO_TOLERANCE) {
      idle_weight = 0.0;
    }
    
    printToSerial(idle_weight, 0, "Idle", false);
  }
}

// Helper function to print clean serial data
void printToSerial(float weight, int currentPwm, String statusMsg, bool forcePrint) {
  unsigned long currentMillis = millis();
  lastWeight = weight;
  lastPwm = currentPwm;
  lastStatus = statusMsg;
  
  if (forcePrint || (currentMillis - lastSerialPrintTime >= 250)) {
    lastSerialPrintTime = currentMillis;
    
    Serial.print("Status: ");
    Serial.print(statusMsg);
    Serial.print(" \t| PWM: ");
    Serial.print(currentPwm);
    Serial.print(" \t| Weight: ");
    Serial.print(weight, 1);
    Serial.println(" g");
  }
}