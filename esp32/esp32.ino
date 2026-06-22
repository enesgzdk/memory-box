#include <WiFi.h>
#include <WiFiManager.h>
#include <Firebase_ESP_Client.h>
#include "time.h"

#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// =====================
// PIN (Harici Elemanlar)
// =====================
#define LED1_PIN 4      // Harici 1. LED (D4)
#define LED2_PIN 5      // Harici 2. LED (D5)
#define RESET_BUTTON 14 // 🔥 WiFi Reset Butonu (D14)

// 🔥 ULN2003A Motor Sürücü Pinleri
const int IN1 = 18;
const int IN2 = 19;
const int IN3 = 21;
const int IN4 = 22;

// =====================
// FIREBASE
// =====================
#define DATABASE_URL "https://memory-box-c2ef8-default-rtdb.europe-west1.firebasedatabase.app/"
// 🔑 Firebase Konsolu -> Veritabanı Sırları kısmından aldığın kodu buraya yapıştır:
#define DATABASE_SECRET "BURAYA_DATABASE_SECRET_GELECEK" 

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

// 🔁 Slayt Modu Değişkenleri
bool slideMode = false;
int slideInterval = 60; // Saniye cinsinden (Siteden gelen veri)
unsigned long slideTimer = 0; // Slayt zamanını sayan dâhili kronometre

unsigned long lastCheck = 0;

// =====================
// CONST
// =====================
// 28BYJ-48 motor için Yarım Adım modunda 45 derece dönmek tam 512 adıma denk gelir.
const int STEP_ANGLE = 512; 
const int MAX_INDEX = 8;

// =====================
// WIFI CONNECT
// =====================
void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false); // 🔥 stability fix

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
  Serial.println(time(nullptr));
}

// =====================
// 🔥 ÇİFT YÖNLÜ VE HIZLI MOTOR DÖNÜŞ FONKSİYONU (YARIM ADIM)
// =====================
void rotateMotor(int totalSteps, int speed, bool clockwise) {
  // Siteden gelen 1-100 arası hız değerini, motorun yarım adım gecikmesine eşliyoruz.
  int adimGecikmesi = map(speed, 1, 100, 3000, 850);

  Serial.print("Motor donuyor... Yon: ");
  Serial.print(clockwise ? "ILERI" : "GERI");
  Serial.print(" | Hedef Adim: ");
  Serial.println(totalSteps);

  for (int i = 0; i < totalSteps; i++) {
    // İleri dönüşte adımlar 0'dan 7'ye akar, geri dönüşte 7'den 0'a geriler.
    int adim = clockwise ? (i % 8) : (7 - (i % 8));
    
    // 8 Aşamalı Sürücü Tetikleme Dizisi
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
    yield(); // WiFi kopmalarını engellemek için arka plan işlerine izin ver
  }

  // Motor bittikten sonra enerji harcamasın ve ısınmasın diye tüm bobinleri kapatıyoruz
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);

  Serial.println("Motor donusu tamamlandi.");
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
  
  // Motor pinlerini çıkış olarak ayarlıyoruz
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  // 🔥 Buton pinini dâhili Pull-up direnciyle giriş olarak ayarlıyoruz
  pinMode(RESET_BUTTON, INPUT_PULLUP);

  // =====================
  // WIFI FIRST
  // =====================
  connectWiFi();

  // =====================
  // CRITICAL: TIME BEFORE FIREBASE
  // =====================
  syncTime();

  // =====================
  // FIREBASE CONFIG
  // =====================
  config.database_url = DATABASE_URL;
  config.signer.tokens.legacy_token = DATABASE_SECRET; 

  config.timeout.serverResponse = 10 * 1000;
  config.timeout.socketConnection = 10 * 1000;

  delay(2000); 

  Firebase.begin(&config, NULL); 
  Firebase.reconnectWiFi(true);

  Serial.println("Firebase init done");
  
  // İlk açılışta zamanlayıcıyı eşitle
  slideTimer = millis();
}

// =====================
// LOOP
// =====================
void loop() {
  
  // =====================
  // 🔥 WIFI RESET KONTROLÜ
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

    digitalWrite(LED1_PIN, LOW);
    digitalWrite(LED2_PIN, LOW);

    if (resetTriggered) {
      WiFiManager wm;
      wm.resetSettings();
      delay(1000);
      ESP.restart();
    } else {
      digitalWrite(LED1_PIN, ledState);
      digitalWrite(LED2_PIN, ledState);
    }
  }

  // =====================
  // 📡 FIREBASE JSON OKUMA DÖNGÜSÜ
  // =====================
  if (Firebase.ready() && millis() - lastCheck > 1000) {
    lastCheck = millis();

    Serial.println("Sorgulanıyor...");

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

      // 🔁 Slayt modu verilerini çekiyoruz
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

        // Kalibrasyon işlemi varsayılan olarak ileri yönde çalışır
        rotateMotor(steps, speedVal, true);

        photoIndex = 1;
        lastPhotoIndex = 1;
        slideTimer = millis(); // Kullanıcı müdahale ettiği için kronometreyi sıfırla

        Firebase.RTDB.setInt(&fbdo, "/device/photoIndex", 1);
        Firebase.RTDB.setBool(&fbdo, "/device/calibrate", false);
        Serial.println("Calibration DONE");
      }

      // =====================
      // ROTATION (Siteden Elle Değişim - EN KISA YOL VE ÇİFT YÖN)
      // =====================
      if (photoIndex != lastPhotoIndex && !calibrate) {
        Serial.println("\n=== ROTATION ===");
        
        int diff = photoIndex - lastPhotoIndex;
        bool clockwise = true;

        // En kısa dairesel rotayı bulma optimizasyonu
        if (diff > MAX_INDEX / 2) {
          diff = diff - MAX_INDEX;
        } else if (diff < -MAX_INDEX / 2) {
          diff = diff + MAX_INDEX;
        }

        // Çıkan farkın işaretine göre yön tayini
        if (diff < 0) {
          clockwise = false;
          diff = -diff; // Adım döngüsü için negatif değeri pozitife çekiyoruz
        }

        int steps = diff * STEP_ANGLE;

        Serial.print("From ");
        Serial.print(lastPhotoIndex);
        Serial.print(" → ");
        Serial.print(photoIndex);
        Serial.print(" | Yön: ");
        Serial.println(clockwise ? "İLERİ" : "GERİ");

        rotateMotor(steps, speedVal, clockwise);
        lastPhotoIndex = photoIndex;
        slideTimer = millis(); // Siteden elle seçim yapıldığında slayt süresini sıfırla
      }

      // =====================
      // LED
      // =====================
      digitalWrite(LED1_PIN, ledState);
      digitalWrite(LED2_PIN, ledState);

    } else {
      Serial.print("Firebase JSON Okuma Hatası: ");
      Serial.println(fbdo.errorReason());
    }
  }

  // =====================
  // 🔁 Otomatik Slayt Modu Kontrolü
  // =====================
  if (slideMode && !calibrate) {
    if (millis() - slideTimer >= ((unsigned long)slideInterval * 1000)) {
      slideTimer = millis(); // Kronometreyi sonraki tur için hemen sıfırla
      
      int nextIndex = photoIndex + 1;
      int steps = STEP_ANGLE;
      
      // 🖤 Siyah yüzeyi (8. yüzü) atlama kontrolü:
      if (photoIndex == 7) {
        nextIndex = 1;
        steps = STEP_ANGLE * 2; // 7'den 1'e direkt zıplamak için 2 katı adım atar
        Serial.println("\n[SLIDE SHOW] 7. Fotoğraftan 1. Fotoğrafa geçiliyor (Siyah yüzey atlandı).");
      } 
      else if (nextIndex > MAX_INDEX) {
        nextIndex = 1;
      }

      Serial.print("\n[SLIDE SHOW] Süre Doldu! Yeni Fotoğrafa Geçiliyor: ");
      Serial.print(photoIndex);
      Serial.print(" → ");
      Serial.println(nextIndex);

      // Slayt akışı her zaman ileri yönde devam eder
      rotateMotor(steps, speedVal, true);
      
      // State'leri güncelle
      photoIndex = nextIndex;
      lastPhotoIndex = nextIndex;

      // 📡 Sitenin de kayması için yeni index'i Firebase'e raporla
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