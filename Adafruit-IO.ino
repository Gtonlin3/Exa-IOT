#include <WiFi.h>
#include "AdafruitIO_WiFi.h"
#include <ESP32Servo.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ==========================
// CONFIGURACIÓN ADAFRUIT IO
// ==========================
#define IO_USERNAME  "Gtonlin3"
#define IO_KEY       "key"
#define WIFI_SSID    "2k18"
#define WIFI_PASS    "delunoalocho"

AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);

// ==========================
// FEEDS DE ADAFRUIT IO
// ==========================
AdafruitIO_Feed *luz_izquierda = io.feed("luz_izquierda");
AdafruitIO_Feed *luz_derecha   = io.feed("luz_derecha");
AdafruitIO_Feed *angulo_servo  = io.feed("angulo_servo");
AdafruitIO_Feed *promedio_luz  = io.feed("promedio_luz");

// ==========================
// PANTALLA OLED
// ==========================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ==========================
// PINES Y VARIABLES
// ==========================
const int LDR_IZQ = 34;
const int LDR_DER = 35;
const int SERVO_PIN = 25;

Servo servo;
int angulo = 90;                    // posición inicial
unsigned long lastUpdate = 0;       // para controlar envíos

// ==========================
// CONFIGURACIÓN INICIAL
// ==========================
void setup() {
  Serial.begin(115200);

  // --- SERVO ---
  servo.attach(SERVO_PIN, 500, 2400); // rango PWM para ESP32
  servo.write(90);                    // posición central

  // --- OLED ---
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("❌ No se detecta la OLED");
    for (;;);
  }

  // --- CONEXIÓN A ADAFRUIT IO ---
  Serial.println("Conectando a Adafruit IO...");
  io.connect();
  while (io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\n✅ Conectado a Adafruit IO!");
}

// ==========================
// LOOP PRINCIPAL
// ==========================
void loop() {
  io.run();  // Mantiene conexión activa

  // --- Lectura de sensores ---
  int luzIzq = analogRead(LDR_IZQ);
  int luzDer = analogRead(LDR_DER);
  int promedio = (luzIzq + luzDer) / 2;
  int diferencia = luzIzq - luzDer;

  // --- Control automático del servo ---
  if (abs(diferencia) > 50) {
    if (diferencia > 0 && angulo > 0) angulo -= 2;       // más luz a la izquierda
    else if (diferencia < 0 && angulo < 180) angulo += 2; // más luz a la derecha
    servo.write(angulo);
  }

  // --- Mostrar en OLED ---
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.print("Luz Izq: "); display.println(luzIzq);
  display.print("Luz Der: "); display.println(luzDer);
  display.print("Angulo: ");  display.println(angulo);
  display.print("Promedio: "); display.println(promedio);
  display.display();

  // --- Envío a Adafruit IO cada 15 segundos ---
  if (millis() - lastUpdate > 15000) {
    luz_izquierda->save(luzIzq);
    luz_derecha->save(luzDer);
    angulo_servo->save(angulo);
    promedio_luz->save(promedio);
    lastUpdate = millis();

    Serial.println("📡 Datos enviados a Adafruit IO:");
    Serial.print("Izq="); Serial.print(luzIzq);
    Serial.print(" | Der="); Serial.print(luzDer);
    Serial.print(" | Prom="); Serial.print(promedio);
    Serial.print(" | Angulo="); Serial.println(angulo);
  }

  delay(200); // estabiliza lectura sin saturar Wi-Fi
}
