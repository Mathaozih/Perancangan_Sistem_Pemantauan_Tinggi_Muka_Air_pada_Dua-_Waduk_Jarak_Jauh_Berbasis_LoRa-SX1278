/*
 * ============================================
 * PROGRAM PENERIMA (RX) + TELEGRAM NOTIFIKASI
 * ============================================
 */

#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>


// === KONFIGURASI WIFI & TELEGRAM (ISI INI) ===
const char* ssid       = "AndroidAP4219";
const char* password   = "wwwww123";
const char* botToken   = "8304452667:AAG6jl1CHXmbULrQi0MjuUirbi_wjFOwfaQ"; // Contoh: 123456:ABC-DEF1234ghIkl-zyx57W2v1u123ew11
const char* chatId     = "6200742914";   // Contoh: 123456789 (Gunakan IDbot untuk cari tahu)

// ==========================================
// 2. KONFIGURASI PIN (Sesuai Skema Anda)
// ==========================================
#define SDA_PIN 21
#define SCL_PIN 22
#define LORA_SCK 18
#define LORA_MISO 19
#define LORA_MOSI 23
#define LORA_CS 5
#define LORA_RST 14
#define LORA_DIO0 2  // Penting! Harus terhubung ke GPIO 2

// ==========================================
// 3. INISIALISASI OBJEK
// ==========================================
WiFiClientSecure client;
UniversalTelegramBot bot(botToken, client);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Struct Data (Wajib sama dengan Pengirim)
struct SensorData {
  int nodeId;
  float percentage;
  int status; // 1=Aman, 2=Waspada, 3=Bahaya
};
SensorData dataTerima;

// Variabel Penyimpanan Data
float persenTX1 = -1.0;
int statusTX1 = 0;
int lastStatusTX1 = 0; // Untuk cek perubahan status

float persenTX2 = -1.0;
int statusTX2 = 0;
int lastStatusTX2 = 0; // Untuk cek perubahan status

// Timer untuk Cek Pesan Telegram (Non-blocking)
unsigned long lastTelegramCheck = 0;
const unsigned long telegramInterval = 2000; // Cek perintah Telegram setiap 2 detik

// ==========================================
// SETUP
// ==========================================
void setup() {
  Serial.begin(115200);

  // 1. Setup LCD
  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.init();
  lcd.backlight();
  lcd.print("System Booting..");
  
  // 2. Setup WiFi
  lcd.setCursor(0, 1);
  lcd.print("Connect WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Tambahkan sertifikat SSL Telegram

  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 15) {
    delay(500);
    Serial.print(".");
    retry++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
    lcd.clear();
    lcd.print("WiFi OK!");
    bot.sendMessage(chatId, "🤖 Sistem Monitoring Air ONLINE!", "");
  } else {
    Serial.println("\nWiFi Failed (Lanjut Offline mode)");
    lcd.clear();
    lcd.print("WiFi GAGAL!");
  }
  delay(1000);

  // 3. Setup LoRa
  lcd.clear();
  lcd.print("Mulai LoRa...");
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa Gagal!");
    lcd.setCursor(0, 1);
    lcd.print("LoRa ERROR!");
    while (1);
  }
  
  // HAPUS SyncWord agar default (0x34) sama dengan TX
  LoRa.receive();
  Serial.println("LoRa RX Siap.");
  lcd.clear();
  lcd.print("Menunggu Data...");
  updateDisplay();
}

// ==========================================
// LOOP UTAMA
// ==========================================
void loop() {
  // --- TUGAS 1: BACA LORA (REALTIME) ---
  int packetSize = LoRa.parsePacket();
  if (packetSize == sizeof(SensorData)) {
    LoRa.readBytes((uint8_t*)&dataTerima, sizeof(dataTerima));

    // Proses Data TX1
    if (dataTerima.nodeId == 1) {
      persenTX1 = dataTerima.percentage;
      statusTX1 = dataTerima.status;
      
      // Logika Notifikasi: Hanya kirim jika status BERUBAH
      if (statusTX1 != lastStatusTX1) {
        kirimNotifikasi(1, persenTX1, statusTX1);
        lastStatusTX1 = statusTX1; 
      }
    } 
    // Proses Data TX2
    else if (dataTerima.nodeId == 2) {
      persenTX2 = dataTerima.percentage;
      statusTX2 = dataTerima.status;
      
      // Logika Notifikasi: Hanya kirim jika status BERUBAH
      if (statusTX2 != lastStatusTX2) {
        kirimNotifikasi(2, persenTX2, statusTX2);
        lastStatusTX2 = statusTX2;
      }
    }
    
    // Update LCD langsung saat data masuk (Realtime)
    updateDisplay();
  }

  // --- TUGAS 2: CEK PERINTAH TELEGRAM (INTERVAL) ---
  // Hanya dijalankan jika WiFi terhubung
  if (WiFi.status() == WL_CONNECTED) {
    if (millis() - lastTelegramCheck > telegramInterval) {
      int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
      while (numNewMessages) {
        handleNewMessages(numNewMessages);
        numNewMessages = bot.getUpdates(bot.last_message_received + 1);
      }
      lastTelegramCheck = millis();
    }
  }
}

// ==========================================
// FUNGSI PENDUKUNG
// ==========================================

// 1. Update Tampilan LCD (Rapi 16 Karakter)
void updateDisplay() {
  lcd.clear();
  char buf[17];
  char statusStr[8];

  // Baris 1: TX1
  lcd.setCursor(0, 0);
  lcd.print("T1:");
  if (persenTX1 < 0) lcd.print(" --.-% OFF");
  else {
    if (statusTX1 == 1) strcpy(statusStr, "AMAN");
    else if (statusTX1 == 2) strcpy(statusStr, "WASPADA");
    else strcpy(statusStr, "BAHAYA");
    snprintf(buf, 14, "%5.1f%%%s", persenTX1, statusStr);
    lcd.print(buf);
  }

  // Baris 2: TX2
  lcd.setCursor(0, 1);
  lcd.print("T2:");
  if (persenTX2 < 0) lcd.print(" --.-% OFF");
  else {
    if (statusTX2 == 1) strcpy(statusStr, "AMAN");
    else if (statusTX2 == 2) strcpy(statusStr, "WASPADA");
    else strcpy(statusStr, "BAHAYA");
    snprintf(buf, 14, "%5.1f%%%s", persenTX2, statusStr);
    lcd.print(buf);
  }
}

// 2. Kirim Notifikasi Otomatis (Saat Status Berubah)
void kirimNotifikasi(int id, float persen, int status) {
  if (WiFi.status() != WL_CONNECTED) return;

  String msg = "⚠️ *PERUBAHAN STATUS TANGKI " + String(id) + "*\n";
  msg += "🌊 Level: " + String(persen, 1) + "%\n";
  msg += "🔔 Status: ";
  
  if (status == 1) msg += "✅ AMAN";
  else if (status == 2) msg += "⚠️ WASPADA";
  else msg += "🚨 BAHAYA!!";

  bot.sendMessage(chatId, msg, "Markdown");
}

// 3. Baca Pesan Masuk (Fitur /cek)
void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;

    // Respon hanya untuk /cek, /status, atau /start
    if (text == "/cek" || text == "/status" || text == "/start") {
      String report = "📊 *MONITORING REPORT*\n\n";
      
      // Laporan T1
      report += "1️⃣ *Tangki 1*: ";
      if (persenTX1 < 0) report += "❌ OFFLINE\n";
      else {
        report += String(persenTX1, 1) + "% ";
        if (statusTX1==1) report += "(Aman) ✅\n";
        else if (statusTX1==2) report += "(Waspada) ⚠️\n";
        else report += "(BAHAYA!) 🚨\n";
      }

      // Laporan T2
      report += "\n2️⃣ *Tangki 2*: ";
      if (persenTX2 < 0) report += "❌ OFFLINE\n";
      else {
        report += String(persenTX2, 1) + "% ";
        if (statusTX2==1) report += "(Aman) ✅\n";
        else if (statusTX2==2) report += "(Waspada) ⚠️\n";
        else report += "(BAHAYA!) 🚨\n";
      }
      
      bot.sendMessage(chat_id, report, "Markdown");
    }
  }
}
