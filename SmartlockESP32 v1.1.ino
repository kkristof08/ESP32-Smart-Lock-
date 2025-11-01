/*
  ESP32 Smart Lock (simple version)
  - Wi-Fi feedback: LED 3x blink
  - Firebase auth feedback: LED 5x blink
  - On any command: LED 3x blink
  - Energize-to-unlock: state.lockX = true only while coil is ON, then false
  - Library: Firebase_ESP_Client v4.4.17
*/

#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>

// ====================== USER SETTINGS ======================
#define WIFI_SSID       "Vodaphone-653B"
#define WIFI_PASSWORD   "9Q7T4k3KpV4jA9Qm"

#define API_KEY         "AIzaSyBUWd_Q8quITSyB7ugvJS7FyQGMD1IGtI0"
#define DATABASE_URL    "https://esp32-lock-api-default-rtdb.europe-west1.firebasedatabase.app/"

#define USER_EMAIL      "esp32device@default.com"
#define USER_PASSWORD   "router"

// Device ID used in RTDB paths
#define DEVICE_ID       "esp32-01"

// Pins (adjust to your wiring)
#define PIN_LOCK_A      16
#define PIN_LOCK_B      17

// Onboard LED pin (most ESP32-WROOM dev boards use GPIO 2; change if needed)
#define LED_PIN         2

// Simulate without hardware (no GPIO switching)
#define DRY_RUN         false

// Pulse bounds (ms) aligned with your RTDB rules
#define PULSE_MIN_MS    100
#define PULSE_MAX_MS    1200

// Heartbeat period (ms)
#define HEARTBEAT_MS    30000
// ===========================================================

// Firebase globals
FirebaseData fbdo;
FirebaseData stream;
FirebaseAuth auth;
FirebaseConfig config;

// DB paths
String pathCmd     = String("/commands/") + DEVICE_ID + "/lastCmd";
String pathState   = String("/devices/")  + DEVICE_ID + "/state";
String pathOnline  = String("/devices/")  + DEVICE_ID + "/online";

// Timers
uint32_t lastHeartbeat = 0;

// ---------- Helpers (decl) ----------
void ledInit();
void ledOn(bool on);
void ledBlink(uint8_t count, uint16_t onMs = 120, uint16_t offMs = 120);

uint32_t clampPulse(uint32_t ms);
void setCoil(uint8_t pin, bool on);

void writeState(bool a, bool b);
void doPulse(uint8_t pin, const char* key, uint32_t ms);
void handleCommandJson(FirebaseJson &j);
void beginStream();
void heartbeat();

// -----------------------------------
void setup() {
  Serial.begin(115200);
  delay(50);

  pinMode(PIN_LOCK_A, OUTPUT);
  pinMode(PIN_LOCK_B, OUTPUT);
  setCoil(PIN_LOCK_A, false);
  setCoil(PIN_LOCK_B, false);
  ledInit();

  // Wi-Fi
  Serial.printf("Connecting to Wi-Fi: %s\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  uint32_t ws = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
    if (millis() - ws > 30000) {
      Serial.println("\nWi-Fi connect timeout. Restarting...");
      ESP.restart();
    }
  }
  Serial.printf("\nWi-Fi connected, IP: %s\n", WiFi.localIP().toString().c_str());
  ledBlink(3, 120, 120); // Wi-Fi OK

  // Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Serial.println("Authenticating with Firebase...");
  uint32_t as = millis();
  while (auth.token.uid.length() == 0 && (millis() - as) < 30000) {
    delay(200);
  }
  if (auth.token.uid.length() == 0) {
    Serial.println("Firebase auth failed. Restarting...");
    delay(1500);
    ESP.restart();
  }
  Serial.printf("Firebase auth OK, UID: %s\n", auth.token.uid.c_str());
  ledBlink(5, 100, 100); // Firebase OK

  // Announce online + initial state
  Firebase.RTDB.setBool(&fbdo, pathOnline.c_str(), true);
  writeState(false, false);

  // Start command stream
  beginStream();

  lastHeartbeat = millis();
}

void loop() {
  heartbeat();
}

// ------------------- Stream handling -------------------
void beginStream() {
  Serial.printf("Begin stream: %s\n", pathCmd.c_str());
  if (!Firebase.RTDB.beginStream(&stream, pathCmd.c_str())) {
    Serial.printf("Stream error: %s\n", stream.errorReason().c_str());
  }

  Firebase.RTDB.setStreamCallback(
    &stream,
    [](FirebaseStream data) {
      if (data.dataTypeEnum() == fb_esp_rtdb_data_type_json) {
        FirebaseJson* j = data.to<FirebaseJson*>();
        ledBlink(3, 80, 80); // command received
        handleCommandJson(*j);
      }
    },
    [](bool timeout) {
      if (timeout) Serial.println("Stream timeout, auto-resume");
    }
  );
}

void handleCommandJson(FirebaseJson &j) {
  FirebaseJsonData res;

  String target = "";
  int pulseMs = 500;
  bool done = false;

  if (j.get(res, "target") && res.typeNum != 0) target = res.to<String>();
  if (j.get(res, "pulseMs") && res.typeNum != 0) pulseMs = res.to<int>();
  if (j.get(res, "done")    && res.typeNum != 0) done    = res.to<bool>();

  if (target.length() == 0) {
    Serial.println("Command missing 'target'");
    return;
  }
  if (done) {
    Serial.println("Command already done; ignoring");
    return;
  }

  uint32_t dur = clampPulse((uint32_t)pulseMs);
  Serial.printf("Execute: target=%s, pulseMs=%u\n", target.c_str(), dur);

  bool doA = (target == "A" || target == "BOTH");
  bool doB = (target == "B" || target == "BOTH");

  // Simple, sequential pulses (safe for small PSUs)
  if (doA) doPulse(PIN_LOCK_A, "lockA", dur);
  if (doB) doPulse(PIN_LOCK_B, "lockB", dur);

  // Ack: done = true
  Firebase.RTDB.setBool(&fbdo, (pathCmd + "/done").c_str(), true);
}

// ------------------- Actuation -------------------
void doPulse(uint8_t pin, const char* key, uint32_t ms) {
  // state true while energized
  FirebaseJson stOn;
  stOn.set(key, true);
  stOn.set("lastUpdate", (int)millis());
  Firebase.RTDB.updateNode(&fbdo, pathState.c_str(), &stOn);

  setCoil(pin, true);
  delay(ms);
  setCoil(pin, false);

  FirebaseJson stOff;
  stOff.set(key, false);
  stOff.set("lastUpdate", (int)millis());
  Firebase.RTDB.updateNode(&fbdo, pathState.c_str(), &stOff);
}

void writeState(bool a, bool b) {
  FirebaseJson st;
  st.set("lockA", a);
  st.set("lockB", b);
  st.set("lastUpdate", (int)millis());
  Firebase.RTDB.updateNode(&fbdo, pathState.c_str(), &st);
}

void setCoil(uint8_t pin, bool on) {
  if (DRY_RUN) {
    Serial.printf("DRY_RUN: pin %u -> %s\n", pin, on ? "ON" : "OFF");
    return;
  }
  digitalWrite(pin, on ? HIGH : LOW);
}

// ------------------- Heartbeat -------------------
void heartbeat() {
  uint32_t now = millis();
  if (now - lastHeartbeat >= HEARTBEAT_MS) {
    lastHeartbeat = now;
    Firebase.RTDB.setBool(&fbdo, pathOnline.c_str(), true);
    FirebaseJson st;
    st.set("lastUpdate", (int)now);
    Firebase.RTDB.updateNode(&fbdo, pathState.c_str(), &st);
  }
}

// ------------------- LED helpers -------------------
void ledInit() {
  pinMode(LED_PIN, OUTPUT);
  ledOn(false);
}
void ledOn(bool on) {
  digitalWrite(LED_PIN, on ? HIGH : LOW);
}
void ledBlink(uint8_t count, uint16_t onMs, uint16_t offMs) {
  for (uint8_t i = 0; i < count; i++) {
    ledOn(true);  delay(onMs);
    ledOn(false); delay(offMs);
  }
}

// ------------------- Utils -------------------
uint32_t clampPulse(uint32_t ms) {
  if (ms < PULSE_MIN_MS) return PULSE_MIN_MS;
  if (ms > PULSE_MAX_MS) return PULSE_MAX_MS;
  return ms;
}
