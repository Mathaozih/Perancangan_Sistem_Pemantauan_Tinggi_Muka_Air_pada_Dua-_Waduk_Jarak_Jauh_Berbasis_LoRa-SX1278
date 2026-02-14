/*
 * ============================================
 * PROGRAM PENGIRIM 2 (TX2) [VERSI PERBAIKAN SENSOR]
 * Node ID: 2
 * Logika sensor HC-SR04 sudah diperbaiki.
 * ============================================
 */

#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// === KONFIGURASI PIN ===
#define TRIG_PIN 4
#define ECHO_PIN 26
#define LED_HI 12
#define LED_WA 13
#define LED_BR 25
#define BUZZER_PIN 27
#define SDA_PIN 21
#define SCL_PIN 22
#define LORA_SCK 18
#define LORA_MISO 19
#define LORA_MOSI 23
#define LORA_CS 5
#define LORA_RST 14
#define LORA_DIO0 2

// === PENGATURAN GLOBAL ===
const int NODE_ID = 2; // <-- ID PENTING (BEDA!)
const float TANGKI_TINGGI = 100.0; 
LiquidCrystal_I2C lcd(0x27, 16, 2);

// [BAGIAN SAMA 1] - STRUCT WAJIB SAMA
struct SensorData {
  int nodeId;
  float percentage;
  int status; // 1=Aman, 2=Waspada, 3=Bahaya
};

SensorData dataKirim;
unsigned long previousMillis = 0;
const long interval = 500;

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LED_HI, OUTPUT);
  pinMode(LED_WA, OUTPUT);
  pinMode(LED_BR, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.init();
  lcd.backlight();
  lcd.print("Pengirim TX2"); // <-- IDENTITAS (BEDA!)
  lcd.setCursor(0, 1);
  lcd.print("Node 2: Mulai..."); // <-- IDENTITAS (BEDA!)

  Serial.println("Mulai LoRa TX2..."); // <-- BEDA
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);
  
  // [BAGIAN SAMA 2 & 3] - FREKUENSI & SYNC WORD
  if (!LoRa.begin(433E6)) { 
    Serial.println("Gagal mulai LoRa!");
    while (1);
  }
  // Tidak ada LoRa.setSyncWord() agar pakai default 0x34

  Serial.println("TX2 (Node 2) Siap."); // <-- BEDA
  delay(1000);
}

void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    // ==============================================================
    // 1. BACA SENSOR (PERBAIKAN UNTUK HC-SR04)
    // ==============================================================
    long durasi;
    float jarak, tinggi, persen;

    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    durasi = pulseIn(ECHO_PIN, HIGH, 30000); // 30ms timeout

    // PERBAIKAN BARU (KHUSUS HC-SR04):
    // Jika durasi = 0 (timeout), anggap tangki kosong.
    if (durasi == 0) { 
      jarak = TANGKI_TINGGI + 1.0; // Set jarak lebih besar dari tinggi tangki
    } else {
      // Jika durasi valid, hitung seperti biasa
      jarak = durasi * 0.034 / 2;
    }
    
    tinggi = TANGKI_TINGGI - jarak;
    persen = constrain((tinggi / TANGKI_TINGGI) * 100, 0, 100);
    // ==============================================================


    // 2. Kontrol Aktuator Lokal
    int status = 1;
    digitalWrite(LED_HI, LOW);
    digitalWrite(LED_WA, LOW);
    digitalWrite(LED_BR, LOW);
    digitalWrite(BUZZER_PIN, LOW);

    if (persen < 30) { status = 1; digitalWrite(LED_HI, HIGH); }
    else if (persen < 70) { status = 2; digitalWrite(LED_WA, HIGH); }
    else { status = 3; digitalWrite(LED_BR, HIGH); digitalWrite(BUZZER_PIN, HIGH); }

    // 3. Update LCD Lokal
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Tangki 2: "); 
    lcd.print(persen, 1);
    lcd.print("%");
    lcd.setCursor(0, 1);
    if (status == 1) lcd.print("AMAN");
    else if (status == 2) lcd.print("WASPADA");
    else lcd.print("BAHAYA");

    // 4. Kirim Data via LoRa
    dataKirim.nodeId = NODE_ID;
    dataKirim.percentage = persen;
    dataKirim.status = status;

    LoRa.beginPacket();
    LoRa.write((uint8_t*)&dataKirim, sizeof(dataKirim));
    LoRa.endPacket();

    Serial.printf("TX2 Kirim: Node %d, Persen: %.1f, Status: %d\n", dataKirim.nodeId, dataKirim.percentage, dataKirim.status);
  }
}