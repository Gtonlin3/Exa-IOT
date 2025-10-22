#include <WiFi.h>
#include <WiFiClientSecure.h> // MUY IMPORTANTE
#include <PubSubClient.h>

// --- MODIFICA ESTO CON TUS DATOS ---
const char* ssid = "SSID";
const char* password = "PASS";

// El nombre corto de tu aplicación (según tu foto: "esp_32")
const char* app_version = "d3s5npam6fhc73ag8hm0"; 

// El Token LARGO que te dio Kaa al crear el "Device"
const char* endpoint_token = "bca8b5a2-8eb5-4a58-b655-c261f78fb54a"; 
// -------------------------------------

// --- Datos de Kaa ---
const char* mqtt_server = "mqtt.kaaiot.com"; 
const int mqtt_port = 8883; // Puerto SSL

// --- Pines de Sensores ---
const int potPin = 34;
const int ldrPin = 35;
const int lm35Pin = 32;
const int trigPin = 26;
const int echoPin = 25;

// --- Variables Globales ---
char mqtt_topic[200];
WiFiClientSecure espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;

void setup() {
  Serial.begin(115200);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  
  setup_wifi();
  
  // Construir el "topic" MQTT específico de Kaa
  snprintf(mqtt_topic, 200, "kp1/%s/dcx/%s/json", app_version, endpoint_token);
  
  Serial.println("--------------------");
  Serial.print("Topic MQTT: ");
  Serial.println(mqtt_topic);
  Serial.println("--------------------");
  
  client.setServer(mqtt_server, mqtt_port);
  
  // Necesario para SSL en el ESP32
  espClient.setInsecure(); 
}

void setup_wifi() {
  delay(10);
  Serial.print("Conectando a ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado");
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Intentando conexión MQTT a Kaa...");
    
    // Conectar a Kaa:
    // ID de cliente (puede ser cualquiera)
    // Usuario: TU ENDPOINT_TOKEN
    // Contraseña: VACÍA ("")
    if (client.connect("ESP32_Cliente", endpoint_token, "")) {
      Serial.println("¡Conectado a Kaa!");
    } else {
      Serial.print("falló, rc=");
      Serial.print(client.state());
      Serial.println(" intentando de nuevo en 5 segundos");
      delay(5000);
    }
  }
}

// Función para leer el sensor ultrasónico
float leerDistancia() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH);
  float distance = duration * 0.034 / 2;
  return distance;
}

// Función para leer el LM35
float leerTemperatura() {
  int adcVal = analogRead(lm35Pin);
  float voltaje = adcVal * (3.3 / 4095.0); // ADC de 12 bits (4095)
  float temperatura = voltaje * 100.0;
  return temperatura;
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop(); // Mantener viva la conexión MQTT

  unsigned long now = millis();
  // Enviar datos cada 5 segundos (5000 ms)
  if (now - lastMsg > 5000) {
    lastMsg = now;

    // --- Lectura de sensores ---
    int valorPot = analogRead(potPin);
    int valorLDR = analogRead(ldrPin);
    float valorTemp = leerTemperatura();
    float valorDist = leerDistancia();

    // --- Crear el mensaje JSON ---
    // Los nombres ("potenciometro", "luz", etc.) son los que
    // Kaa aprenderá automáticamente gracias a "Autoextract"
    char jsonPayload[200];
    snprintf(jsonPayload, 200, 
      "{\"potenciometro\": %d, \"luz\": %d, \"temperatura\": %.2f, \"distancia\": %.2f}",
      valorPot, 
      valorLDR, 
      valorTemp, 
      valorDist
    );

    // --- Publicar el mensaje ---
    Serial.print("Publicando mensaje: ");
    Serial.println(jsonPayload);
    
    client.publish(mqtt_topic, jsonPayload);
  }
}
