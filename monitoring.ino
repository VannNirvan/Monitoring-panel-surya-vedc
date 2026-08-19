#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_INA219.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>

// ---------------- KONFIGURASI OLED ----------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SDA_PIN 21
#define SCL_PIN 22

Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_INA219 ina219;

// ---------------- KONFIGURASI WIFI ----------------
const char* ssid     = "BOE";
const char* password = ""; // BOE open network, tidak pakai password

const int WIFI_CONNECT_TIMEOUT_MS = 20000; // batas waktu tunggu koneksi awal (20 detik)
const unsigned long WIFI_CHECK_INTERVAL = 10000; // cek status WiFi tiap 10 detik
unsigned long lastWifiCheck = 0;

// ---------------- KONFIGURASI FIREBASE ----------------
#define FIREBASE_API_KEY       "FIREBASE API KEY"
#define FIREBASE_DATABASE_URL  "https://monitoring-panel-surya-vedc-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_USER_EMAIL    "YOUR USER EMAIL"
#define FIREBASE_USER_PASSWORD "YOUR USER PASSWORD"

FirebaseData   fbdo;
FirebaseAuth   auth;
FirebaseConfig config;

// Nilai sensor terbaru (global, dipakai bareng OLED & Firebase)
float g_voltage = 0;
float g_current = 0;
float g_power   = 0;

unsigned long lastUpdate = 0;
const unsigned long UPDATE_INTERVAL = 500; // ms, jeda baca sensor & update OLED

const unsigned long FIREBASE_SEND_INTERVAL = 2000; // ms, jeda kirim data ke Firebase
unsigned long lastFirebaseSend = 0;

// Fungsi bantu: coba konek WiFi dengan batas waktu, update OLED sambil menunggu
bool connectWiFi() {
  WiFi.begin(ssid, password);

  unsigned long startAttempt = millis();
  int dotCount = 0;

  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - startAttempt > WIFI_CONNECT_TIMEOUT_MS) {
      Serial.println("\nGagal konek WiFi (timeout)");
      return false;
    }

    delay(500);
    Serial.print(".");

    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Connecting to SSID");
    display.print(ssid);
    for (int i = 0; i < dotCount; i++) display.print(".");
    display.display();

    dotCount = (dotCount + 1) % 4;
  }

  Serial.println("\nWiFi terhubung!");
  Serial.println(WiFi.localIP());
  return true;
}

// ---------------- INIT FIREBASE ----------------
void initFirebase() {
  config.api_key = FIREBASE_API_KEY;
  config.database_url = FIREBASE_DATABASE_URL;

  auth.user.email    = FIREBASE_USER_EMAIL;
  auth.user.password = FIREBASE_USER_PASSWORD;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Serial.println("Firebase diinisialisasi.");
}

// ---------------- KIRIM DATA KE FIREBASE ----------------
void sendToFirebase() {
  if (!Firebase.ready()) return;

  // Pencegahan jika nilai sensor terbaca NaN
  if (isnan(g_voltage)) g_voltage = 0.0;
  if (isnan(g_current)) g_current = 0.0;
  if (isnan(g_power))   g_power = 0.0;

  FirebaseJson json;
  json.set("voltage", g_voltage);
  json.set("current", g_current);
  json.set("power", g_power);
  json.set("ip", WiFi.localIP().toString());

  if (Firebase.RTDB.setJSON(&fbdo, "/plts/current", &json)) {
    Serial.println("Data Firebase berhasil dikirim.");
  } else {
    Serial.print("Firebase gagal: ");
    Serial.println(fbdo.errorReason());
  }
}

void setup() {
  Serial.begin(115200);

  // Inisialisasi I2C (dipakai bareng OLED & INA219 pada Pin 21 & 22)
  Wire.begin(SDA_PIN, SCL_PIN);

  // Inisialisasi OLED (alamat I2C 0x3C, reset software)
  if (!display.begin(0x3C, true)) {
    Serial.println("SH1106G gagal diinisialisasi");
    for (;;); // stop kalau OLED tidak terdeteksi
  }

  // Inisialisasi INA219
  if (!ina219.begin()) {
    Serial.println("INA219 gagal diinisialisasi");
    for (;;);
  }

  // Tampilkan "connecting to SSID" di OLED
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.println("Connecting to SSID");
  display.println(ssid);
  display.display();

  // Mulai koneksi WiFi dengan timeout
  bool connected = connectWiFi();

  if (!connected) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("WiFi gagal konek");
    display.println("Melanjutkan tanpa WiFi");
    display.println("Akan coba ulang...");
    display.display();
    delay(1500);
  } else {
    initFirebase();
  }

  // Tampilan awal setelah setup selesai
  display.clearDisplay();
  display.display();

  lastWifiCheck = millis();
}

void loop() {
  // Cek status WiFi berkala, reconnect kalau putus (non-blocking)
  unsigned long now = millis();
  if (now - lastWifiCheck >= WIFI_CHECK_INTERVAL) {
    lastWifiCheck = now;
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi terputus, mencoba reconnect...");
      WiFi.disconnect();
      WiFi.reconnect();
    }
  }

  // Baca sensor & update OLED tiap UPDATE_INTERVAL ms (non-blocking)
  if (now - lastUpdate >= UPDATE_INTERVAL) {
    lastUpdate = now;

    // Ambil data dari sensor INA219
    float shuntvoltage = ina219.getShuntVoltage_mV();
    float busvoltage   = ina219.getBusVoltage_V();
    float current_mA   = ina219.getCurrent_mA();

    g_voltage = busvoltage + (shuntvoltage / 1000.0); // Tegangan aktual (V)
    g_current = current_mA / 1000.0;                  // Arus dalam Ampere
    g_power   = g_voltage * g_current;                 // Daya dalam Watt

    // Update OLED (Tampilan Asli Terdokumentasi)
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);

    display.setCursor(0, 0);
    display.print("SSID: ");
    display.println(ssid);

    display.setCursor(0, 10);
    display.print("IP: ");
    if (WiFi.status() == WL_CONNECTED) {
      display.println(WiFi.localIP());
    } else {
      display.println("Tidak terhubung");
    }

    display.setCursor(0, 25);
    display.print("Tegangan: ");
    display.print(g_voltage, 2);
    display.println(" V");

    display.setCursor(0, 37);
    display.print("Arus: ");
    display.print(g_current, 3);
    display.println(" A");

    display.setCursor(0, 49);
    display.print("Daya: ");
    display.print(g_power, 2);
    display.println(" W");

    display.setCursor(0, 56);
    display.print("Firebase: ");
    display.println(Firebase.ready() ? "OK" : "...");

    display.display();
  }

  // Kirim data ke Firebase tiap FIREBASE_SEND_INTERVAL ms (non-blocking)
  if (now - lastFirebaseSend >= FIREBASE_SEND_INTERVAL) {
    lastFirebaseSend = now;
    if (WiFi.status() == WL_CONNECTED) {
      sendToFirebase();
    }
  }
}
