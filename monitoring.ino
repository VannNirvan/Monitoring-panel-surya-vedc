#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <WebServer.h>
#include <Firebase_ESP_Client.h>


// ============================================================
// FIREBASE
// ============================================================

#define API_KEY "AIzaSyBjU9LbSD6Auw5mxxpF3Xv2SSgW_nhA0oM"

#define DATABASE_URL \
"https://iot-daya-monitoring-default-rtdb.asia-southeast1.firebasedatabase.app"

#define USER_EMAIL "404notclient@gmail.com"
#define USER_PASSWORD "NDA0bm90dmFu"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;


// ============================================================
// INTERVAL
// ============================================================

const unsigned long UPDATE_INTERVAL = 200;
unsigned long lastUpdate = 0;

const unsigned long FIREBASE_INTERVAL = 2000;
unsigned long lastFirebaseUpdate = 0;


// ============================================================
// KONFIGURASI OLED
// ============================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDR 0x3C

Adafruit_SH1106G display(
    SCREEN_WIDTH,
    SCREEN_HEIGHT,
    &Wire,
    OLED_RESET
);


// ============================================================
// KONFIGURASI PIN
// ============================================================

const int PIN_POT_VOLT = 36;
const int PIN_POT_ARUS = 39;


// ============================================================
// KONFIGURASI WIFI
// ============================================================

const char* WIFI_SSID = "JIK-2024";
const char* WIFI_PASS = "";


// ============================================================
// WEB SERVER
// ============================================================

WebServer server(80);

String ipAddressStr = "Menghubungkan...";


// ============================================================
// VARIABEL SENSOR
// ============================================================

float tegangan = 0.0;
float arus = 0.0;
float daya = 0.0;


// ============================================================
// HALAMAN HTML
// ============================================================

// PASTIKAN INDEX_HTML ANDA MASIH ADA DI SINI
// jika web lokal masih ingin digunakan.


// ============================================================
// HANDLER WEB SERVER
// ============================================================

// ============================================================
// WIFI
// ============================================================

void connectWiFi() {

    WiFi.mode(WIFI_STA);

    WiFi.begin(
        WIFI_SSID,
        WIFI_PASS
    );

    Serial.print("Menghubungkan ke WiFi \"");
    Serial.print(WIFI_SSID);
    Serial.print("\" ");

    unsigned long startAttempt = millis();

    while (
        WiFi.status() != WL_CONNECTED &&
        millis() - startAttempt < 15000
    ) {

        delay(300);
        Serial.print(".");
    }


    if (WiFi.status() == WL_CONNECTED) {

        ipAddressStr =
            WiFi.localIP().toString();

        Serial.println();

        Serial.print(
            "WiFi terhubung. IP address: "
        );

        Serial.println(
            ipAddressStr
        );

    } else {

        ipAddressStr = "Tidak terhubung";

        Serial.println();

        Serial.println(
            "Gagal terhubung ke WiFi."
        );
    }
}


// ============================================================
// FIREBASE
// ============================================================

void initFirebase() {

    config.api_key = API_KEY;

    config.database_url = DATABASE_URL;

    auth.user.email = USER_EMAIL;

    auth.user.password = USER_PASSWORD;

    Firebase.begin(
        &config,
        &auth
    );

    Firebase.reconnectWiFi(true);

    Serial.println(
        "Firebase diinisialisasi."
    );
}


// ============================================================
// KIRIM DATA KE FIREBASE
// ============================================================

void kirimFirebase() {

    if (!Firebase.ready()) {

        Serial.println(
            "Firebase belum siap."
        );

        return;
    }


    FirebaseJson json;

    json.set(
        "tegangan",
        tegangan
    );

    json.set(
        "arus",
        arus
    );

    json.set(
        "daya",
        daya
    );


    if (
        Firebase.RTDB.setJSON(
            &fbdo,
            "/devices/esp32_01",
            &json
        )
    ) {

        Serial.println(
            "Data Firebase berhasil dikirim."
        );

    } else {

        Serial.print(
            "Firebase gagal: "
        );

        Serial.println(
            fbdo.errorReason()
        );
    }
}


// ============================================================
// SETUP
// ============================================================

void setup() {

    Serial.begin(115200);

    delay(200);


    // ========================================================
    // ADC
    // ========================================================

    analogReadResolution(12);

    pinMode(
        PIN_POT_VOLT,
        INPUT
    );

    pinMode(
        PIN_POT_ARUS,
        INPUT
    );


    // ========================================================
    // OLED
    // ========================================================

    if (
        !display.begin(
            OLED_ADDR,
            true
        )
    ) {

        Serial.println(
            "SH110X tidak terdeteksi!"
        );

        while (true) {
            delay(1000);
        }
    }


    display.clearDisplay();

    display.setTextColor(
        SH110X_WHITE
    );

    display.setTextSize(1);

    display.setCursor(0, 0);

    display.println(
        "Menghubungkan WiFi..."
    );

    display.display();


    // ========================================================
    // WIFI
    // ========================================================

    connectWiFi();


    // ========================================================
    // FIREBASE
    // ========================================================

    if (WiFi.status() == WL_CONNECTED) {

        initFirebase();

    }


    // ========================================================
    // STATUS OLED
    // ========================================================

    display.clearDisplay();

    display.setCursor(0, 0);

    display.println(
        "WiFi: JIK-2024"
    );

    display.setCursor(0, 12);

    display.println(
        WiFi.status() == WL_CONNECTED
            ? "Status: Terhubung"
            : "Status: Gagal"
    );

    display.setCursor(0, 24);

    display.print("IP: ");

    display.println(
        ipAddressStr
    );

    display.display();

    delay(2000);


    // ========================================================
    // WEB SERVER
    // ========================================================

    Serial.print(
        "Buka http://"
    );

    Serial.println(
        ipAddressStr
    );
}


// ============================================================
// BACA SENSOR
// ============================================================

void bacaSensor() {

    int rawVolt =
        analogRead(PIN_POT_VOLT);

    int rawArus =
        analogRead(PIN_POT_ARUS);


    // Tegangan: 1 - 500 V

    tegangan =
        1.0 +
        (
            (float)rawVolt / 4095.0
        ) *
        (500.0 - 1.0);


    // Arus: 0 - 10 A

    arus =
        (
            (float)rawArus / 4095.0
        ) *
        10.0;


    // Daya

    daya =
        tegangan * arus;
}


// ============================================================
// OLED
// ============================================================

void tampilkanOLED() {

    display.clearDisplay();

    display.setTextSize(1);

    display.setCursor(0, 0);

    display.println(
        "MONITOR DAYA LISTRIK"
    );

    display.drawLine(
        0,
        10,
        SCREEN_WIDTH,
        10,
        SH110X_WHITE
    );


    display.setCursor(0, 14);

    display.print("V:");

    display.print(
        tegangan,
        1
    );

    display.print(" I:");

    display.print(
        arus,
        2
    );


    display.setCursor(0, 26);

    display.print("Daya: ");

    display.print(
        daya,
        1
    );

    display.println(
        " W"
    );


    display.setCursor(0, 40);

    display.print("WiFi: ");

    display.println(
        WiFi.status() == WL_CONNECTED
            ? "Online"
            : "Offline"
    );


    display.setCursor(0, 52);

    display.print("IP: ");

    display.print(
        ipAddressStr
    );

    display.display();
}


// ============================================================
// LOOP
// ============================================================

void loop() {
    unsigned long now =
        millis();


    // ========================================================
    // SENSOR + OLED
    // ========================================================

    if (
        now - lastUpdate >=
        UPDATE_INTERVAL
    ) {

        lastUpdate = now;


        bacaSensor();

        tampilkanOLED();


        Serial.print(
            "Tegangan: "
        );

        Serial.print(
            tegangan,
            1
        );


        Serial.print(
            " V | Arus: "
        );

        Serial.print(
            arus,
            2
        );


        Serial.print(
            " A | Daya: "
        );

        Serial.print(
            daya,
            1
        );


        Serial.println(
            " W"
        );
    }


    // ========================================================
    // FIREBASE
    // ========================================================

    if (
        now - lastFirebaseUpdate >=
        FIREBASE_INTERVAL
    ) {

        lastFirebaseUpdate = now;

        kirimFirebase();
    }
}