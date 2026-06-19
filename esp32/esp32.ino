#include <WiFi.h>
#include <WiFiManager.h>
#include <Firebase_ESP_Client.h>

#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// =====================
// PIN
// =====================
#define LED_PIN 2

// =====================
// FIREBASE
// =====================
#define API_KEY "AIzaSyD9yBVo4_AkmdyKwnceY51-PPMTHwJclEI"
#define DATABASE_URL "https://memory-box-c2ef8-default-rtdb.europe-west1.firebasedatabase.app/"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// =====================
// STATE
// =====================
int photoIndex = 1;
int lastPhotoIndex = 1;
int speedVal = 20;
bool ledState = true;
bool calibrate = false;

unsigned long lastCheck = 0;

// =====================
// CONST
// =====================
const int STEP_ANGLE = 45;
const int MAX_INDEX = 8;

// =====================
// WIFI SETUP (PROVISIONING)
// =====================
void connectWiFi() {

  WiFi.mode(WIFI_STA);

  WiFiManager wm;

  bool res = wm.autoConnect("ESP32_SETUP");

  if (!res) {
    Serial.println("WiFi connection failed");
    ESP.restart();
  }

  Serial.println("WiFi connected");
}

// =====================
// MOTOR SIMULATION
// =====================
void rotateMotor(int steps, int speedVal) {

  int delayTime = map(speedVal, 1, 100, 50, 5);

  for (int i = 0; i < steps; i++) {
    Serial.print("Step ");
    Serial.println(i + 1);

    delay(delayTime);
    yield();
  }

  Serial.println("Motor done");
}

// =====================
// SETUP
// =====================
void setup() {

  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);

  connectWiFi();

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Serial.println("Firebase ready");
}

// =====================
// LOOP
// =====================
void loop() {

  // =====================
  // FIREBASE READ
  // =====================
  if (Firebase.ready() && millis() - lastCheck > 500) {
    lastCheck = millis();

    Firebase.RTDB.getInt(&fbdo, "/device/photoIndex");
    photoIndex = fbdo.intData();

    Firebase.RTDB.getInt(&fbdo, "/device/speed");
    speedVal = fbdo.intData();

    Firebase.RTDB.getBool(&fbdo, "/device/led");
    ledState = fbdo.boolData();

    Firebase.RTDB.getBool(&fbdo, "/device/calibrate");
    calibrate = fbdo.boolData();

    // =====================
    // CALIBRATION
    // =====================
    if (calibrate) {

      Serial.println("\n=== CALIBRATION START ===");

      int targetIndex = 1;

      int diff = targetIndex - lastPhotoIndex;
      if (diff < 0) diff += MAX_INDEX;

      int steps = diff * STEP_ANGLE;

      rotateMotor(steps, speedVal);

      // state reset
      photoIndex = 1;
      lastPhotoIndex = 1;

      // web sync için state update
      Firebase.RTDB.setInt(&fbdo, "/device/photoIndex", 1);
      Firebase.RTDB.setBool(&fbdo, "/device/calibrate", false);

      Serial.println("Calibration DONE");
    }

    // =====================
    // NORMAL ROTATION
    // =====================
    if (photoIndex != lastPhotoIndex) {

      int diff = photoIndex - lastPhotoIndex;
      if (diff < 0) diff += MAX_INDEX;

      int steps = diff * STEP_ANGLE;

      Serial.println("\n=== ROTATION ===");
      Serial.print("From: ");
      Serial.print(lastPhotoIndex);
      Serial.print(" → ");
      Serial.println(photoIndex);

      rotateMotor(steps, speedVal);

      lastPhotoIndex = photoIndex;
    }

    // =====================
    // LED CONTROL
    // =====================
    digitalWrite(LED_PIN, ledState);
  }
}