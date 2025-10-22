#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
// --- MODIFICA ESTO CON TUS DATOS ---
const char* ssid = "CANDY LOKA";
const char* password = "4372287LP";

// La dirección de tu broker EMQX (Paso 1)
const char* mqtt_server = "y92bc1ac.ala.us-east-1.emqxsl.com"; 
// El usuario y contraseña que creaste (Paso 2)
const char* mqtt_user = "mi_esp32";
const char* mqtt_pass = "12345";
// El tópico que te suscribiste (Paso 3)
const char* mqtt_topic = "sensores";

// Puerto MQTT seguro (SSL)
const int mqtt_port = 8883;

// --- Pines de Sensores (Según Parte 2) ---
// Analógicos
const int potPin = 34;
const int ldrPin = 35;
const int lm35Pin = 32;
// Digitales (Ultrasonido)
const int trigPin = 26;
const int echoPin = 25;

// Necesitamos WiFiClientSecure para la conexión SSL al puerto 8883
WiFiClientSecure espClient;
PubSubClient client(espClient);

unsigned long lastMsg = 0;
const int msgInterval = 5000; // Enviar datos cada 5 segundos

void setup() {
  Serial.begin(115200);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  
  setup_wifi();
  
  // Configurar el cliente MQTT
  client.setServer(mqtt_server, mqtt_port);
  
  // Esta línea es necesaria para saltar la verificación del certificado SSL
  // No es lo más seguro para producción, pero es lo más fácil para empezar.
  espClient.setInsecure(); 
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Conectando a ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado");
  Serial.println("IP: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  // Loop hasta reconectar
  while (!client.connected()) {
    Serial.print("Intentando conexión MQTT...");
    // Intentar conectar con usuario y contraseña
    if (client.connect("ESP32_Sensor_Client", mqtt_user, mqtt_pass)) {
      Serial.println("¡Conectado a EMQX!");
      // (Opcional) Suscribirse a un tópico si el ESP32 también necesita recibir comandos
      // client.subscribe("mihogar/luces"); 
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
  // Limpiar el TrigPin
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  // Enviar pulso
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  // Leer el tiempo del pulso
  long duration = pulseIn(echoPin, HIGH);
  // Calcular distancia en cm
  float distance = duration * 0.034 / 2;
  return distance;
}

// Función para leer el LM35 (con ADC de 12 bits del ESP32 y Vref de 3.3V)
float leerTemperatura() {
  int adcVal = analogRead(lm35Pin);
  // Convertir el valor ADC a voltaje (4095 es el máximo para 12 bits)
  float voltaje = adcVal * (3.3 / 4095.0);
  // Convertir voltaje a temperatura (LM35 da 10mV por grado Celsius)
  float temperatura = voltaje * 100.0;
  return temperatura;
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop(); // Mantener viva la conexión MQTT

  unsigned long now = millis();
  if (now - lastMsg > msgInterval) {
    lastMsg = now;

    // --- Lectura de sensores ---
    int valorPot = analogRead(potPin);
    int valorLDR = analogRead(ldrPin);
    float valorTemp = leerTemperatura();
    float valorDist = leerDistancia();

    // --- Crear el mensaje JSON ---
    // Usamos un buffer de char para ser eficientes
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
