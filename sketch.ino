#define BLYNK_TEMPLATE_ID "TMPL6-c4tdJXl"
#define BLYNK_TEMPLATE_NAME "Pengingat Servis Kendaraan"
#define BLYNK_AUTH_TOKEN "FoXdcIGIZbxk-x_YkqTJfVmVmkgbVkAX"

#include <Wire.h>
#include <RTClib.h>
#include <BlynkSimpleEsp32.h>
#include <WiFi.h>
#include <WiFiClient.h>

// === Pin Setup ===
#define ENCODER_CLK 2
#define ENCODER_DT  4

// === RTC ===
RTC_DS1307 rtc;

// === Variabel Encoder & Jarak ===
volatile int encoderPos = 0;
int totalKM = 0;
const int KM_PER_TICK = 1000; // 1 tick = 1000 km

// === Servis Data ===
int kmLastServis = 0;
DateTime timeLastServis;
bool servisNotified = false;

// === Simulasi waktu cepat ===
unsigned long lastSimulasiMillis = 0;
const unsigned long intervalSimulasi = 5000; // 5 detik = 1 bulan

// === WiFi ===
char ssid[] = "Wokwi-GUEST";
char pass[] = "";

// === Virtual Pins ===
#define VPIN_TOTAL_KM V0
#define VPIN_KM_SINCE V1
#define VPIN_MONTHS_SINCE V2
#define VPIN_STATUS V3
#define VPIN_RESET_BUTTON V4

// === Encoder Interrupt ===
void IRAM_ATTR readEncoder() {
  if (digitalRead(ENCODER_CLK) == digitalRead(ENCODER_DT)) {
    encoderPos++;
  } else {
    encoderPos--;
  }
}

// === Reset servis via Blynk ===
BLYNK_WRITE(VPIN_RESET_BUTTON) {
  int value = param.asInt();
  if (value == 1) {
    kmLastServis = totalKM;
    timeLastServis = rtc.now();
    servisNotified = false;
    Serial.println("RESET SERVIS via Blynk");
  }
}

void setup() {
  Serial.begin(115200);

  // === Encoder Setup ===
  pinMode(ENCODER_CLK, INPUT);
  pinMode(ENCODER_DT, INPUT);
  attachInterrupt(digitalPinToInterrupt(ENCODER_CLK), readEncoder, CHANGE);

  // === RTC Setup ===
  if (!rtc.begin()) {
    Serial.println("RTC tidak terdeteksi");
    while (1);
  }
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  // === Servis Awal ===
  timeLastServis = rtc.now();
  kmLastServis = 0;

  // === WiFi Manual Connect ===
  Serial.print("Menghubungkan ke WiFi...");
  WiFi.begin(ssid, pass);
  int wifiCounter = 0;
  while (WiFi.status() != WL_CONNECTED && wifiCounter < 20) {
    delay(500);
    Serial.print(".");
    wifiCounter++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi Terhubung");
  } else {
    Serial.println("\n❌ Gagal Terhubung ke WiFi");
    return;
  }

  // === Blynk Manual Connect ===
  Serial.println("Menghubungkan ke Blynk...");
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  unsigned long blynkTimeout = millis();
  while (!Blynk.connected() && millis() - blynkTimeout < 10000) {
    Blynk.run();
  }

  if (Blynk.connected()) {
    Serial.println("✅ Terhubung ke Blynk!");
  } else {
    Serial.println("❌ Gagal terhubung ke Blynk.");
  }
}

void loop() {
  Blynk.run();

  // === Simulasi waktu cepat ===
  if (millis() - lastSimulasiMillis >= intervalSimulasi) {
    lastSimulasiMillis = millis();

    DateTime now = rtc.now();
    DateTime next = now + TimeSpan(30, 0, 0, 0); // Tambah 30 hari
    rtc.adjust(next);

    Serial.print("Simulasi waktu +1 bulan: ");
    Serial.println(next.timestamp());
  }

  // === Update KM dari encoder ===
  static int lastPos = 0;
  if (encoderPos != lastPos) {
    int delta = encoderPos - lastPos;
    totalKM += delta * KM_PER_TICK;
    lastPos = encoderPos;

    // Tambahan: tampilkan ke Serial
    Serial.print("Encoder berubah: ");
    Serial.print(delta);
    Serial.print(" tick, Total KM: ");
    Serial.println(totalKM);
  }

  // === Cek Waktu dan Jarak ===
  DateTime now = rtc.now();
  TimeSpan sinceServis = now - timeLastServis;
  int kmSinceServis = totalKM - kmLastServis;
  int bulanSejakServis = sinceServis.days() / 30;

  // === Status Servis ===
  bool servisDue = (kmSinceServis >= 5000) || (bulanSejakServis >= 5);

  // === Kirim ke Blynk ===
  Blynk.virtualWrite(VPIN_TOTAL_KM, totalKM);
  Blynk.virtualWrite(VPIN_KM_SINCE, kmSinceServis);
  Blynk.virtualWrite(VPIN_MONTHS_SINCE, bulanSejakServis);
  Blynk.virtualWrite(VPIN_STATUS, servisDue ? "SERVIS!" : "Normal");

  // === Notifikasi jika waktunya servis ===
  if (servisDue && !servisNotified) {
    Blynk.logEvent("servis_due", "Kendaraan perlu diservis!");
    servisNotified = true;
  } else if (!servisDue) {
    servisNotified = false;
  }

  delay(200);
}
