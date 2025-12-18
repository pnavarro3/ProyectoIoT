/*
 * ESP32 - Sistema de Sensores con MQTT
 * Sensores: Ultrasonidos, Luminosidad (LDR), DHT11 (Temp/Humedad)
 * Transmite datos via MQTT al montacargas
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

// ========== CONFIGURACIÓN WIFI ==========
const char* ssid = "yiyiyi";
const char* password = "xabicrack";

// ========== CONFIGURACIÓN MQTT ==========
// IMPORTANTE: Cambia esta IP por la de tu portátil cuando lo configures
// Para obtenerla, ejecuta en PowerShell: ipconfig
// Busca la IPv4 de tu adaptador WiFi/Ethernet (ejemplo: 192.168.1.XXX)
const char* mqtt_server = "10.160.243.29";  // IP del portátil
const int mqtt_port = 1883;
const char* mqtt_user = "";                  // Usuario MQTT (opcional)
const char* mqtt_password = "";              // Contraseña MQTT (opcional)
const char* mqtt_client_id = "ESP32_Sensores";

// Tópicos MQTT
const char* topic_distancia = "montacargas/sensores/distancia";
const char* topic_luminosidad = "montacargas/sensores/luminosidad";
const char* topic_temperatura = "montacargas/sensores/temperatura";
const char* topic_humedad = "montacargas/sensores/humedad";
const char* topic_estado = "montacargas/sensores/estado";

// ========== PINES DE SENSORES ==========
// Sensor Ultrasonidos HC-SR04
#define TRIG_PIN 5
#define ECHO_PIN 18

// Sensor de Luminosidad (LDR con umbral)
#define LDR_PIN 34  // Pin digital (HIGH=Oscuro, LOW=Claro)

// LED de iluminación
#define LED_LUZ 15  // LED que se enciende cuando está oscuro

// Sensor DHT11
#define DHT_PIN 16  // GPIO16 (cambiado para mejor compatibilidad)
#define DHT_TYPE DHT11

// ========== OBJETOS ==========
WiFiClient espClient;
PubSubClient mqttClient(espClient);
DHT dht(DHT_PIN, DHT_TYPE);

// ========== VARIABLES GLOBALES ==========
unsigned long lastSensorRead = 0;
const long sensorInterval = 2000;  // Intervalo de lectura de sensores (2 segundos)

float distancia = 0.0;
int luminosidad = 0;
float temperatura = 0.0;
float humedad = 0.0;

// Buffer para filtro de mediana del ultrasonidos
const int NUM_MUESTRAS = 5;  // Número de muestras para la mediana
float muestrasDistancia[NUM_MUESTRAS];

// ========== FUNCIONES ==========

// Función de ordenamiento burbuja para mediana
void ordenarArray(float arr[], int n) {
  for (int i = 0; i < n - 1; i++) {
    for (int j = 0; j < n - i - 1; j++) {
      if (arr[j] > arr[j + 1]) {
        float temp = arr[j];
        arr[j] = arr[j + 1];
        arr[j + 1] = temp;
      }
    }
  }
}

// Calcular mediana de un array
float calcularMediana(float arr[], int n) {
  float tempArr[n];
  // Copiar array para no modificar el original
  for (int i = 0; i < n; i++) {
    tempArr[i] = arr[i];
  }
  
  ordenarArray(tempArr, n);
  
  // Si es impar, retornar el valor del medio
  if (n % 2 != 0) {
    return tempArr[n / 2];
  }
  // Si es par, retornar el promedio de los dos valores centrales
  return (tempArr[(n - 1) / 2] + tempArr[n / 2]) / 2.0;
}

void setup() {
  Serial.begin(115200);
  
  // Configurar pines
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LDR_PIN, INPUT);  // Pin digital
  pinMode(LED_LUZ, OUTPUT); // LED de iluminación
  digitalWrite(LED_LUZ, LOW); // Apagado inicialmente
  
  // Inicializar DHT11
  dht.begin();
  
  // Conectar a WiFi
  conectarWiFi();
  
  // Configurar MQTT
  mqttClient.setServer(mqtt_server, mqtt_port);
  
  Serial.println("Sistema de sensores iniciado");
  Serial.print("Intervalo de muestreo: ");
  Serial.print(sensorInterval);
  Serial.println(" ms");
  Serial.print("Muestras por mediana: ");
  Serial.println(NUM_MUESTRAS);
}

void loop() {
  // Mantener conexión MQTT
  if (!mqttClient.connected()) {
    reconectarMQTT();
  }
  mqttClient.loop();
  
  // Leer sensores periódicamente
  unsigned long currentMillis = millis();
  if (currentMillis - lastSensorRead >= sensorInterval) {
    lastSensorRead = currentMillis;
    
    leerSensores();
    publicarDatos();
  }
}

void conectarWiFi() {
  Serial.print("Conectando a WiFi");
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println();
  Serial.println("WiFi conectado");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void reconectarMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Conectando a MQTT...");
    
    if (mqttClient.connect(mqtt_client_id, mqtt_user, mqtt_password)) {
      Serial.println("conectado");
      mqttClient.publish(topic_estado, "ESP32 Sensores conectado");
    } else {
      Serial.print("falló, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" reintentando en 5 segundos");
      delay(5000);
    }
  }
}

void leerSensores() {
  // Leer sensor de ultrasonidos
  distancia = leerUltrasonidos();
  
  // Leer sensor de luminosidad (LDR)
  luminosidad = leerLuminosidad();
  
  // Leer sensor DHT11
  temperatura = dht.readTemperature();
  humedad = dht.readHumidity();
  
  // Validar lecturas DHT11
  if (isnan(temperatura) || isnan(humedad)) {
    Serial.println("Error leyendo DHT11");
    temperatura = -1;
    humedad = -1;
  }
  
  // Mostrar lecturas en Serial
  Serial.println("========== LECTURAS (Mediana) ==========");
  Serial.print("Distancia: ");
  Serial.print(distancia);
  Serial.println(" cm");
  
  Serial.print("Luminosidad: ");
  Serial.print(luminosidad == 1 ? "Oscuro" : "Claro");
  Serial.print(" (");
  Serial.print(luminosidad);
  Serial.println(")");
  
  Serial.print("Temperatura: ");
  Serial.print(temperatura);
  Serial.println(" °C");
  
  Serial.print("Humedad: ");
  Serial.print(humedad);
  Serial.println(" %");
  Serial.println("==============================");
}

float leerUltrasonidos() {
  // Tomar múltiples muestras para calcular mediana
  for (int i = 0; i < NUM_MUESTRAS; i++) {
    // Enviar pulso
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    
    // Leer eco
    long duracion = pulseIn(ECHO_PIN, HIGH, 30000);  // Timeout 30ms
    
    // Calcular distancia en cm
    if (duracion > 0) {
      muestrasDistancia[i] = duracion * 0.034 / 2;
    } else {
      muestrasDistancia[i] = -1;  // Lectura inválida
    }
    
    delay(10);  // Pequeña pausa entre lecturas
  }
  
  // Filtrar valores inválidos (fuera de rango 2-400 cm)
  int muestrasValidas = 0;
  float muestrasLimpias[NUM_MUESTRAS];
  
  for (int i = 0; i < NUM_MUESTRAS; i++) {
    if (muestrasDistancia[i] >= 2 && muestrasDistancia[i] <= 400) {
      muestrasLimpias[muestrasValidas] = muestrasDistancia[i];
      muestrasValidas++;
    }
  }
  
  // Si no hay suficientes muestras válidas, retornar -1
  if (muestrasValidas < 3) {
    return -1;
  }
  
  // Calcular y retornar mediana de las muestras válidas
  return calcularMediana(muestrasLimpias, muestrasValidas);
}

int leerLuminosidad() {
  // Leer valor digital del LDR (HIGH=Oscuro, LOW=Claro)
  int estadoLDR = digitalRead(LDR_PIN);
  
  // Controlar LED: encender si está oscuro, apagar si está claro
  if (estadoLDR == HIGH) {
    digitalWrite(LED_LUZ, HIGH);  // Encender LED (oscuro)
  } else {
    digitalWrite(LED_LUZ, LOW);   // Apagar LED (claro)
  }
  
  // Devolver 1 si está oscuro, 0 si está claro
  return estadoLDR;
}

void publicarDatos() {
  char mensaje[50];
  
  // Verificar conexión MQTT antes de publicar
  if (!mqttClient.connected()) {
    Serial.println("MQTT desconectado, no se pueden publicar datos");
    return;
  }
  
  // Publicar distancia
  if (distancia >= 0) {
    dtostrf(distancia, 6, 2, mensaje);
    mqttClient.publish(topic_distancia, mensaje);
  }
  
  // Publicar luminosidad
  snprintf(mensaje, sizeof(mensaje), "%d", luminosidad);
  mqttClient.publish(topic_luminosidad, mensaje);
  
  // Publicar temperatura
  if (temperatura >= 0) {
    dtostrf(temperatura, 6, 2, mensaje);
    mqttClient.publish(topic_temperatura, mensaje);
  }
  
  // Publicar humedad
  if (humedad >= 0) {
    dtostrf(humedad, 6, 2, mensaje);
    mqttClient.publish(topic_humedad, mensaje);
  }
  
  // Imprimir por serial cada vez que se publican datos
  Serial.println("\n========== DATOS ENVIADOS ==========");
  Serial.print("Distancia: "); Serial.print(distancia); Serial.println(" cm");
  Serial.print("Luminosidad: "); Serial.println(luminosidad == 0 ? "Claro" : "Oscuro");
  Serial.print("Temperatura: "); Serial.print(temperatura); Serial.println(" °C");
  Serial.print("Humedad: "); Serial.print(humedad); Serial.println(" %");
  Serial.println("====================================");
}
