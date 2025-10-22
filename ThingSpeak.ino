#include <WiFi.h>
#include <ThingSpeak.h>
#include <max6675.h>   // para la termocupla MAX6675

// ===== WiFi =====
const char* ssid     = "ssid";
const char* password = "password";

// ===== ThingSpeak =====
unsigned long channelNumber = 3116876;         // <-- TU Channel ID
const char*   writeAPIKey    = "J7VDULE4ZJEXQYMX";// <-- TU Write API Key
WiFiClient client;

// ===== Pines (ESP32) =====
const int LM35_PIN = 34;   // LM35DZ (ADC1)
const int LDR_PIN  = 35;   // LDR (ADC1)
const int POT_PIN  = 32;   // Potenciómetro (ADC1)

// MAX6675 (Termocupla K)
const int SCK_PIN = 18;
const int CS_PIN  = 5;
const int SO_PIN  = 19;
MAX6675 tc(SCK_PIN, CS_PIN, SO_PIN);

// ===== Intervalo de envío =====
const unsigned long SEND_MS = 20000;  // >= 15000 ms (límite free)
unsigned long lastSend = 0;

// ----- Helpers -----
static inline int pctFromAdc(int pin) {
  int raw = analogRead(pin);                // 0..4095
  int pct = (int)((raw / 4095.0f) * 100.0f);
  if (pct < 0) pct = 0; if (pct > 100) pct = 100;
  return pct;
}

static inline float readLM35C() {
  int raw = analogRead(LM35_PIN);           // 0..4095
  float mV = (raw / 4095.0f) * 3300.0f;     // mV @3.3V
  return mV / 10.0f;                        // 10 mV/°C
}

void setup() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(250); }

  ThingSpeak.begin(client);

  analogReadResolution(12);
  analogSetPinAttenuation(LM35_PIN, ADC_11db); // lectura estable 0..3.3V
}

void loop() {
  if (millis() - lastSend >= SEND_MS) {
    int   potPct = pctFromAdc(POT_PIN);     // % (0..100)
    int   ldrPct = pctFromAdc(LDR_PIN);     // % (si te queda invertido usa: 100 - pctFromAdc(LDR_PIN))
    float lm35C  = readLM35C();             // °C
    float tcC    = tc.readCelsius();        // °C (MAX6675)

    ThingSpeak.setField(1, potPct);
    ThingSpeak.setField(2, ldrPct);
    ThingSpeak.setField(3, lm35C);
    ThingSpeak.setField(4, tcC);
    ThingSpeak.writeFields(channelNumber, writeAPIKey);

    lastSend = millis();
  }
}
