#include <WiFi.h>
#include <WiFiManager.h>
#include <Firebase_ESP_Client.h>
#include "time.h"
#include <Preferences.h> // 🔥 1. İÇ HAFIZA KİTAPLIĞINI EKLEDİK

#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// =====================
// PIN (Harici Elemanlar)
// =====================
#define LED1_PIN 4      
#define LED2_PIN 5      
#define RESET_BUTTON 14 

// 🔥 ULN2003A Motor Sürücü Pinleri
const int IN1 = 18;
const int IN2 = 19;
const int IN3 = 21;
const int IN4 = 22;

// =====================
// FIREBASE
// =====================
#define DATABASE_URL "https://memory-box-c2ef8-default-rtdb.europe-west1.firebasedatabase.app/"
#define DATABASE_SECRET "BURAYA_DATABASE_SECRET_GELECEK" 

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// =====================
// STATE & MEMORY
// =====================
Preferences preferences; // 🔥 2. HAFIZA NESNESİNİ TANIMLADIK

int photoIndex = 1;
int lastPhotoIndex = 1;
int speedVal = 20;
bool ledState = true;
bool calibrate = false;

bool slideMode = false;
int slideInterval = 60; 
unsigned long slideTimer = 0; 

unsigned long lastCheck = 0;

// =====================
// CONST
// =====================
const int STEP_ANGLE = 512; 
const int MAX_INDEX = 8;

// =====================
// WIFI CONNECT
// =====================
void connectWiFi() {
  WiFi.mode(WIFI_STA);
  
  // 🔥 3. KRİTİK: WiFiManager'ın arkada her saniye flash hafızaya yazmasını engelliyoruz!
  // Sadece RAM'de tutacak, hafıza ömrü erimeyecek.
  WiFi.persistent(false); 
  
  WiFi.setSleep(false); 

  WiFiManager wm;
  bool res = wm.autoConnect("Memory_Box_Setup");

  if (!res) {
    Serial.println("WiFi failed");
    ESP.restart();
  }

  Serial.println("WiFi connected");
  Serial.println(WiFi.localIP());
}

// =====================
// NTP SYNC (SSL FIX)
// =====================
void syncTime() {
  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("Syncing time");
  time_t now = time(nullptr);
  int retry = 0;
  while (now < 1700000000 && retry < 20) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
    retry++;
  }
  Serial.println("\nTime synced OK");
}

// =====================
// MOTOR DÖNÜŞ FONKSİYONU
// =====================
void rotateMotor(int totalSteps, int speed, bool clockwise) {
  int adimGecikmesi = map(speed, 1, 100, 3000, 850);
  for (int i = 0; i < totalSteps; i++) {
    int adim = clockwise ? (i % 8) : (7 - (i % 8));
    switch(adim) {
      case 0: digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);  digitalWrite(IN3, LOW);  digitalWrite(IN4, LOW);  break;
      case 1: digitalWrite(IN1, HIGH); digitalWrite(IN2, HIGH); digitalWrite(IN3, LOW);  digitalWrite(IN4, LOW);  break;
      case 2: digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH); digitalWrite(IN3, LOW);  digitalWrite(IN4, LOW);  break;
      case 3: digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH); digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);  break;
      case 4: digitalWrite(IN1, LOW);  digitalWrite(IN2, LOW);  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);  break;
      case 5: digitalWrite(IN1, LOW);  digitalWrite(IN2, LOW);  digitalWrite(IN3, HIGH); digitalWrite(IN4, HIGH); break;
      case 6: digitalWrite(IN1, LOW);  digitalWrite(IN2, LOW);  digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH); break;
      case 7: digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);  digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH); break;
    }
    delayMicroseconds(adimGecikmesi);
    yield(); 
  }
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW); digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
}

// =====================
// SETUP
// =====================
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\nBOOT START");

  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT); pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pinMode(RESET_BUTTON, INPUT_PULLUP);

  // 🔥 4. İÇ HAFIZAYI BAŞLATIYORUZ VE SON KALDIĞI YERİ OKUYORUZ
  preferences.begin("mbox_ram", false); // "mbox_ram" isimli bir klasör açtık
  
  // Cihaz elektrik kesintisinden uyandığında Firebase'e sormadan önce hafızadan son indexi okur.
  // Eğer hafıza bomboşsa (ilk açılışsa) varsayılan olarak 1 kabul eder.
  photoIndex = preferences.getInt("savedIndex", 1);
  lastPhotoIndex = photoIndex; 
  
  Serial.print("🔥 [HAFIZA] Cihaz hafizadan uykudan uyandi. Son Konum: ");
  Serial.println(photoIndex);

  connectWiFi();
  syncTime();

  config.database_url = DATABASE_URL;
  config.signer.tokens.legacy_token = DATABASE_SECRET; 
  config.timeout.serverResponse = 10 * 1000;
  config.timeout.socketConnection = 10 * 1000;
  delay(2000); 

  Firebase.begin(&config, NULL); 
  Firebase.reconnectWiFi(true);
  Serial.println("Firebase init done");
  
  // 🔥 5. İlk açılışta Firebase'i hafızadan okuduğumuz güncel index ile eşitliyoruz
  if(Firebase.ready()){
     Firebase.RTDB.setInt(&fbdo, "/device/photoIndex", photoIndex);
     Serial.println("🔥 [HAFIZA] Firebase senkronizasyonu ilk acilista yapildi.");
  }

  slideTimer = millis();
}

// =====================
// LOOP
// =====================
void loop() {
  
  // =====================
  // WIFI RESET KONTROLÜ
  // =====================
  if (digitalRead(RESET_BUTTON) == LOW) {
    Serial.println("Reset butonuna basildi, kontrol ediliyor...");
    unsigned long pressTime = millis();
    unsigned long lastBlinkTime = 0;
    bool currentAnimLedState = LOW;
    bool resetTriggered = false;
    
    while (digitalRead(RESET_BUTTON) == LOW) {
      if (millis() - lastBlinkTime > 500) {
        lastBlinkTime = millis();
        currentAnimLedState = !currentAnimLedState;
        digitalWrite(LED1_PIN, currentAnimLedState);
        digitalWrite(LED2_PIN, currentAnimLedState);
      }
      if (millis() - pressTime > 3000) {
        Serial.println("\n[WIFI RESET] Hafiza siliniyor ve cihaz yeniden baslatiliyor...");
        resetTriggered = true;
        break;
      }
      delay(50);
    }

    digitalWrite(LED1_PIN, LOW); digitalWrite(LED2_PIN, LOW);

    if (resetTriggered) {
      // 🔥 6. WiFi resetlenirken bizim tuttuğumuz konum hafızasını da sıfırlıyoruz
      preferences.putInt("savedIndex", 1);
      
      WiFiManager wm;
      wm.resetSettings();
      delay(1000);
      ESP.restart();
    } else {
      digitalWrite(LED1_PIN, ledState);   digitalWrite(LED2_PIN, ledState);
    }
  }

  // =====================
  // 📡 FIREBASE JSON OKUMA DÖNGÜSÜ
  // =====================
  if (Firebase.ready() && millis() - lastCheck > 1000) {
    lastCheck = millis();

    if (Firebase.RTDB.getJSON(&fbdo, "/device")) {
      FirebaseJson &json = fbdo.jsonObject();
      FirebaseJsonData jsonData;

      json.get(jsonData, "photoIndex");
      if (jsonData.success) photoIndex = jsonData.intValue;

      json.get(jsonData, "speed");
      if (jsonData.success) speedVal = jsonData.intValue;

      json.get(jsonData, "led");
      if (jsonData.success) ledState = jsonData.boolValue;

      json.get(jsonData, "calibrate");
      if (jsonData.success) calibrate = jsonData.boolValue;

      json.get(jsonData, "slideMode");
      if (jsonData.success) slideMode = jsonData.boolValue;

      json.get(jsonData, "slideInterval");
      if (jsonData.success) slideInterval = jsonData.intValue;

      // =====================
      // CALIBRATION (1. Yüze Sıfırlama)
      // =====================
      if (calibrate) {
        Serial.println("\n=== CALIBRATION START ===");
        int targetIndex = 1;
        int diff = targetIndex - lastPhotoIndex;
        if (diff < 0) diff += MAX_INDEX;
        int steps = diff * STEP_ANGLE;

        rotateMotor(steps, speedVal, true);

        photoIndex = 1;
        lastPhotoIndex = 1;
        slideTimer = millis(); 

        // 🔥 7. Kalibrasyon bitince hafızayı da 1. konuma çekiyoruz
        preferences.putInt("savedIndex", 1);

        Firebase.RTDB.setInt(&fbdo, "/device/photoIndex", 1);
        Firebase.RTDB.setBool(&fbdo, "/device/calibrate", false);
        Serial.println("Calibration DONE");
      }

      // =====================
      // ROTATION (Siteden Elle Değişim)
      // =====================
      if (photoIndex != lastPhotoIndex && !calibrate) {
        Serial.println("\n=== ROTATION ===");
        int diff = photoIndex - lastPhotoIndex;
        bool clockwise = true;

        if (diff > MAX_INDEX / 2) { diff = diff - MAX_INDEX; } 
        else if (diff < -MAX_INDEX / 2) { diff = diff + MAX_INDEX; }

        if (diff < 0) {
          clockwise = false;
          diff = -diff; 
        }

        int steps = diff * STEP_ANGLE;
        rotateMotor(steps, speedVal, clockwise);
        
        lastPhotoIndex = photoIndex;
        slideTimer = millis(); 

        // 🔥 8. Siteden elle motor döndürüldüğünde yeni konumu hafızaya kilitliyoruz
        preferences.putInt("savedIndex", photoIndex);
        Serial.print("🔥 [HAFIZA] Yeni konum kaydedildi: ");
        Serial.println(photoIndex);
      }

      digitalWrite(LED1_PIN, ledState);
      digitalWrite(LED2_PIN, ledState);

    }
  }

  // =====================
  // 🔁 Otomatik Slayt Modu Kontrolü
  // =====================
  if (slideMode && !calibrate) {
    if (millis() - slideTimer >= ((unsigned long)slideInterval * 1000)) {
      slideTimer = millis(); 
      
      int nextIndex = photoIndex + 1;
      int steps = STEP_ANGLE;
      
      if (photoIndex == 7) {
        nextIndex = 1;
        steps = STEP_ANGLE * 2; 
        Serial.println("\n[SLIDE SHOW] 7. Fotoğraftan 1. Fotoğrafa geçiliyor (Siyah yüzey atlandı).");
      } 
      else if (nextIndex > MAX_INDEX) {
        nextIndex = 1;
      }

      rotateMotor(steps, speedVal, true);
      
      photoIndex = nextIndex;
      lastPhotoIndex = nextIndex;

      // 🔥 9. Otomatik slayt modunda da yeni konumu hafızaya yazıyoruz
      preferences.putInt("savedIndex", photoIndex);

      if (Firebase.ready()) {
        Firebase.RTDB.setInt(&fbdo, "/device/photoIndex", nextIndex);
        Serial.println("[SLIDE SHOW] Yeni index Firebase'e gönderildi.");
      }
    }
  } else {
    if (!slideMode) {
      slideTimer = millis();
    }
  }

  delay(20); 
}