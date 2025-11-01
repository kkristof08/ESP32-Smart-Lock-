#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>   // Provides the tokenStatusCallback()

// ---------- USER SETTINGS ----------
#define WIFI_SSID       "Vodafone-653B"
#define WIFI_PASSWORD   "9Q7T4k3KpV4jA9Qm"

#define API_KEY         "AIzaSyBUWd_Q8quITSyB7ugvJS7FyQGMD1IGtI0"
#define DATABASE_URL    "https://esp32-lock-api-default-rtdb.europe-west1.firebasedatabase.app"

#define USER_EMAIL      "esp32device@default.com"
#define USER_PASSWORD   "router"
// -----------------------------------

#define LED_PIN 2   // Built-in LED pin

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Helper to blink LED
void blinkLED(int times, int delayMs) {
    for (int i = 0; i < times; i++) {
        digitalWrite(LED_PIN, HIGH);
        delay(delayMs);
        digitalWrite(LED_PIN, LOW);
        delay(delayMs);
    }
}

// Sync NTP time for HTTPS
void syncTime() {
    Serial.println("Syncing NTP time...");
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    time_t now = 0;
    int retry = 0;
    while (now < 1000000000 && retry < 40) { // wait for valid time
        delay(250);
        Serial.print(".");
        now = time(nullptr);
        retry++;
    }
    Serial.println();
    if (now > 1000000000) {
        Serial.printf("Time synced: %s\n", ctime(&now));
    } else {
        Serial.println("Time sync FAILED (TLS may fail).");
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    Serial.println("\n=== ESP32 Firebase Diagnostic ===");

    // --- Connect Wi-Fi ---
    Serial.printf("Connecting to Wi-Fi SSID: %s\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    unsigned long startWiFi = millis();
    while (WiFi.status() != WL_CONNECTED) {
        delay(300);
        Serial.print(".");
    }
    unsigned long wifiTime = millis() - startWiFi;
    Serial.printf("\nWi-Fi connected in %.2f seconds\n", wifiTime / 1000.0);
    Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
    blinkLED(3, 150);

    // --- Sync NTP time for SSL ---
    syncTime();

    // --- Configure Firebase ---
    Serial.println("Configuring Firebase...");
    config.api_key = API_KEY;
    config.database_url = DATABASE_URL;
    config.token_status_callback = tokenStatusCallback; // show token events

    auth.user.email = USER_EMAIL;
    auth.user.password = USER_PASSWORD;

    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);

    // --- Wait for token / auth ---
    Serial.println("Connecting to Firebase...");
    unsigned long startFB = millis();
    while (auth.token.uid.length() == 0 && millis() - startFB < 30000) {
        delay(300);
        Serial.print(".");
    }

    if (auth.token.uid.length() > 0) {
        unsigned long fbTime = millis() - startFB;
        Serial.printf("\nFirebase connected in %.2f seconds\n", fbTime / 1000.0);
        Serial.printf("User UID: %s\n", auth.token.uid.c_str());
        blinkLED(5, 100);
    } else {
        Serial.println("\nFirebase did NOT connect within 30 seconds.");
        Serial.printf("Token status: %d\n", (int)config.signer.tokens.status);
        if (config.signer.tokens.error.message.length()) {
            Serial.printf("Token error: %s\n", config.signer.tokens.error.message.c_str());
        }
        Serial.println("Verify API key, user, password, and database URL.");
    }

    Serial.println("\nSystem ready. Checking Firebase every second...");
}

void loop() {
    static const char* PATH = "/devices/esp32-01/blink";
    if (Firebase.ready()) {
        if (Firebase.RTDB.getBool(&fbdo, PATH)) {
            bool blink = fbdo.boolData();
            Serial.printf("Firebase OK -> %s = %s\n", PATH, blink ? "true" : "false");

            if (blink) {
                digitalWrite(LED_PIN, HIGH);
                delay(300);
                digitalWrite(LED_PIN, LOW);
                delay(300);
            } else {
                digitalWrite(LED_PIN, LOW);
            }
        } else {
            Serial.printf("Firebase read failed: %s\n", fbdo.errorReason().c_str());
        }
    } else {
        Serial.println("Firebase not ready (token refreshing or auth in progress)");
    }
    delay(1000);
}