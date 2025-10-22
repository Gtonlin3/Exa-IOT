#include <WiFi.h>
#include <HTTPClient.h>
#include <max6675.h>  // Termocupla MAX6675

/* ====== CONFIG ====== */
const char* WIFI_SSID = "ssid";
const char* WIFI_PASS = "password";

const char* UBIDOTS_TOKEN = "BBUS-IMyVua3Z0GOz9O8za3rcVzqqz6wtTK";
const char* DEVICE_LABEL  = "esp32_lab";   // <-- el de tu dispositivo

// Variable labels EXACTOS en Ubidots
const char* VAR_POT = "potenciometro";
const char* VAR_LDR = "ldr";
const char* VAR_LM35 = "lm35dz";
const char* VAR_TC   = "termocupla";

/* ====== Pines ====== */
const int LM35_PIN = 34;   // LM35DZ
const int LDR_PIN  = 35;   // LDR
const int POT_PIN  = 32;   // Potenciómetro

// MAX6675
const int SCK_PIN = 18, CS_PIN = 5, SO_PIN = 19;
MAX6675 tc(SCK_PIN, CS_PIN, SO_PIN);

/* ====== Intervalo ====== */
const unsigned long SEND_MS = 20000;
unsigned long lastSend = 0;

/* ====== Helpers ====== */
int pctFromAdc(int pin) {
  int raw = analogRead(pin);                 // 0..4095
  int pct = (int)((raw / 4095.0f) * 100.0f);
  if (pct < 0) pct = 0; if (pct > 100) pct = 100;
  return pct;
}
float readLM35C() {
  int raw = analogRead(LM35_PIN);
  float mV = (raw / 4095.0f) * 3300.0f;      // a 3.3 V
  return mV / 10.0f;                         // 10 mV/°C
}

/* ====== Setup ====== */
void setup() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(300); }

  analogReadResolution(12);
  analogSetPinAttenuation(LM35_PIN, ADC_11db); // estable 0..3.3V
}

/* ====== Loop ====== */
void loop() {
  if (millis() - lastSend >= SEND_MS) {
    int   potPct = pctFromAdc(POT_PIN);              // %
    int   ldrPct = pctFromAdc(LDR_PIN);              // %  (si va invertido usa: 100 - pctFromAdc(LDR_PIN))
    float lm35C  = readLM35C();                      // °C
    float tcC    = tc.readCelsius();                 // °C

    // Construir JSON: {"potenciometro_pct":75, "ldr_pct":60, ...}
    String payload = String("{\"") + VAR_POT + "\":" + potPct +
                     ",\"" + VAR_LDR + "\":" + ldrPct +
                     ",\"" + VAR_LM35 + "\":" + String(lm35C,1) +
                     ",\"" + VAR_TC   + "\":" + String(tcC,1) + "}";

    HTTPClient http;
    String url = "http://industrial.api.ubidots.com/api/v1.6/devices/" + String(DEVICE_LABEL);
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Auth-Token", UBIDOTS_TOKEN);
    http.POST(payload);
    http.end();

    lastSend = millis();
  }
}

