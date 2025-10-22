#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <time.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>

// ==========================
// WIFI
// ==========================
const char* WIFI_SSID = "2k18";
const char* WIFI_PASS = "delunoalocho";

// ==========================
// AZURE IOT HUB (Rellena)
// ==========================
// FQDN del IoT Hub: p.ej. "MiHub.azure-devices.net"
const char* IOT_HUB_HOST = "TU_HUB.azure-devices.net";
// Device ID creado en el Hub
const char* DEVICE_ID    = "TU_DEVICE_ID";
// Primary Key (base64) del dispositivo
const char* DEVICE_KEY   = "TU_DEVICE_PRIMARY_KEY_BASE64";

// API version
const char* IOT_API_VER  = "2021-04-12";

// DigiCert Global Root G2 (Azure IoT)
static const char AZURE_ROOT_CA[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDrzCCApegAwIBAgIQBzBVvT6r9bT2vGdPFQf1pzANBgkqhkiG9w0BAQsFADBh
MQswCQYDVQQGEwJVUzETMBEGA1UEChMKRGlnaUNlcnQgSW5jMR8wHQYDVQQLExZE
aWdpQ2VydCBHbG9iYWwgUm9vdCBHMjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTYx
MjAwMDBaMGExCzAJBgNVBAYTAlVTMRMwEQYDVQQKEwpEaWdpQ2VydCBJbmMxHzAd
BgNVBAsTFkRpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG9w0BAQEF
AAOCAQ8AMIIBCgKCAQEAs3Nw4jI0LlwM651c01qmPvvrLpzjAU6YewsG58IOzB8o
B9Qd3s3zK9J9GgYbG7Vso/2jS9G4VZL3B2A+FiYqSdz+CA0nVddOZXS6jttuPAHy
oV+Gqz/mdrIW6DCe4k74vNys8lm2Sleqrs9jwELyhl725LLJoPLD114F8CbnMD4H
W596k8ZZrVSu2Ce279b9Ec/WWEDuJeayQMZT6X0hgqP/d9vywgq6Z9erjRzCQXDp
Kk+ShsY4xXHzkwmo7aX6ixkmKuuU4N2VNivXzXcX4Jt9V7H5PHeTtNKgHdwNwp0K
rGZWlznImPi0tLxe3Ljpsp8dkUBaRdOy+nK8BPVK9QIDAQABo0IwQDAOBgNVHQ8B
Af8EBAMCAQYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUy7v1Gf4bm/F/6Qba
b1Xc5teLoI0wDQYJKoZIhvcNAQELBQADggEBAG3Efr2E1VlzDq+W8sELpAo0P5Nd
BrPl1v48dqH6G+j6lJzi10BGSAdoo6gWQBaIj++ImQxGc1dQc5sKXc5teLoI0lp4
sIwoMvVJE9idh+NangNh4tW7x1YgnPZXoqBYwygJyI072QtdgQXl3k5iADG7n2AF
/3Gzdh0dX8GZFODdgNpTiFqouBZfyqCkCmZJLdnOjFkWDXLI4YAlnXrhIRbkIuAe
GHWxirY6z3Y0JVpaz6RtZpmjHtkobaN6D+PfYZ7R6pujISiFDUFxIr05oig3NbS1
YPj3TDIrS9KuX3sI5OGucs5cjox96D65gis6pZeRAEIJ5zsxFrqOeE0zY4xXH8=
-----END CERTIFICATE-----
)EOF";

// ==========================
// OLED + SERVO + SENSORES
// ==========================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const int LDR_IZQ  = 34;  // ADC1
const int LDR_DER  = 35;  // ADC1
const int SERVO_PIN = 25; // PWM

Servo servo;
int angulo = 90;
unsigned long lastUpdate = 0;

// ==========================
// MQTT
// ==========================
WiFiClientSecure net;
PubSubClient mqtt(net);

// mbedTLS utils
#include "mbedtls/base64.h"
#include "mbedtls/md.h"

// URL encode
String urlEncode(const String &s) {
  String out;
  const char *hex = "0123456789ABCDEF";
  for (size_t i = 0; i < s.length(); i++) {
    char c = s.charAt(i);
    if (('a'<=c && c<='z') || ('A'<=c && c<='Z') || ('0'<=c && c<='9') || c=='-'||c=='_'||c=='.'||c=='~') {
      out += c;
    } else {
      out += '%';
      out += hex[(c >> 4) & 0xF];
      out += hex[c & 0xF];
    }
  }
  return out;
}

String b64encode(const uint8_t* data, size_t len) {
  size_t out_len = 4*((len+2)/3) + 4;
  std::vector<unsigned char> out(out_len);
  size_t olen = 0;
  mbedtls_base64_encode(out.data(), out.size(), &olen, data, len);
  return String((char*)out.data());
}

std::vector<uint8_t> b64decode(const String &in) {
  size_t out_len = 0;
  std::vector<uint8_t> out(in.length());
  mbedtls_base64_decode(out.data(), out.size(), &out_len,
                        (const unsigned char*)in.c_str(), in.length());
  out.resize(out_len);
  return out;
}

std::vector<uint8_t> hmac_sha256(const uint8_t* key, size_t key_len,
                                 const uint8_t* msg, size_t msg_len) {
  const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  std::vector<uint8_t> out(32);
  mbedtls_md_hmac(info, key, key_len, msg, msg_len, out.data());
  return out;
}

// SAS token (expira en secondsFromNow)
String makeSasToken(const String& host, const String& deviceId,
                    const String& base64Key, uint32_t secondsFromNow) {
  time_t now = time(nullptr);
  uint32_t se = (uint32_t)now + secondsFromNow;

  String resourceUri = host + "/devices/" + deviceId;
  resourceUri.toLowerCase();
  String stringToSign = urlEncode(resourceUri) + "\n" + String(se);

  auto key = b64decode(base64Key);
  auto mac = hmac_sha256(key.data(), key.size(),
                         (const uint8_t*)stringToSign.c_str(), stringToSign.length());
  String sig = b64encode(mac.data(), mac.size());
  String sigEnc = urlEncode(sig);

  String token = "SharedAccessSignature sr=" + urlEncode(resourceUri)
               + "&sig=" + sigEnc + "&se=" + String(se);
  return token;
}

bool mqttConnect() {
  String username = String(IOT_HUB_HOST) + "/" + DEVICE_ID + "/?api-version=" + IOT_API_VER;
  String password = makeSasToken(IOT_HUB_HOST, DEVICE_ID, DEVICE_KEY, 24*60*60); // 24h

  net.setCACert(AZURE_ROOT_CA);
  mqtt.setServer(IOT_HUB_HOST, 8883);

  Serial.println("🔐 Conectando a Azure IoT Hub (MQTT)...");
  bool ok = mqtt.connect(DEVICE_ID, username.c_str(), password.c_str());
  if (ok) Serial.println("✅ MQTT conectado");
  else    Serial.printf("❌ MQTT rc=%d\n", mqtt.state());
  return ok;
}

void ensureMqtt() {
  if (!mqtt.connected()) mqttConnect();
}

// ==========================
// SETUP
// ==========================
void setup() {
  Serial.begin(115200);

  // Servo
  servo.attach(SERVO_PIN, 500, 2400);
  servo.write(90);

  // OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("❌ No se detecta la OLED");
    for(;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("ESP32 -> Azure IoT Hub");
  display.display();

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Conectando WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(700);
  }
  Serial.printf("\n✅ WiFi OK: %s\n", WiFi.localIP().toString().c_str());

  // NTP para SAS
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("⏱️ Sincronizando NTP");
  time_t now = 0; int tries = 0;
  while (now < 1700000000 && tries < 30) { // espera hasta época válida
    Serial.print(".");
    delay(500);
    now = time(nullptr);
    tries++;
  }
  Serial.printf("\n🕒 Epoch: %ld\n", now);

  mqtt.setBufferSize(1024);
  mqttConnect();
}

// ==========================
// LOOP
// ==========================
void loop() {
  ensureMqtt();
  mqtt.loop();

  // Lecturas
  int luzIzq = analogRead(LDR_IZQ);
  int luzDer = analogRead(LDR_DER);
  int promedio = (luzIzq + luzDer) / 2;
  int diferencia = luzIzq - luzDer;

  // Control servo
  if (abs(diferencia) > 50) {
    if (diferencia > 0 && angulo > 0) angulo -= 2;
    else if (diferencia < 0 && angulo < 180) angulo += 2;
    servo.write(angulo);
  }

  // OLED
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Luz Izq: "); display.println(luzIzq);
  display.print("Luz Der: "); display.println(luzDer);
  display.print("Angulo : "); display.println(angulo);
  display.print("Promedio: "); display.println(promedio);
  display.display();

  // Enviar a Azure cada 15 s
  if (millis() - lastUpdate > 15000 && mqtt.connected()) {
    // Topic telemetría
    String topic = "devices/"; topic += DEVICE_ID; topic += "/messages/events/";

    // JSON
    String payload = "{\"luz_izquierda\":";
    payload += luzIzq;
    payload += ",\"luz_derecha\":";
    payload += luzDer;
    payload += ",\"angulo_servo\":";
    payload += angulo;
    payload += ",\"promedio_luz\":";
    payload += promedio;
    payload += "}";

    bool ok = mqtt.publish(topic.c_str(), payload.c_str());
    Serial.println(ok ? "📡 Telemetria enviada" : "⚠️ Fallo al publicar");
    Serial.println(payload);

    lastUpdate = millis();
  }

  delay(200);
}
