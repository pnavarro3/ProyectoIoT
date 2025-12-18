/*
 * Test DHT11 - Diagnóstico
 */

#include <DHT.h>

#define DHT_PIN 16      // Probamos GPIO16
#define DHT_TYPE DHT11  // Volvemos a DHT11

DHT dht(DHT_PIN, DHT_TYPE);

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n=== TEST DHT11 ===");
  Serial.print("Pin: GPIO");
  Serial.println(DHT_PIN);
  Serial.print("Tipo: ");
  Serial.println(DHT_TYPE == DHT11 ? "DHT11" : "DHT22");
  
  dht.begin();
  delay(2000);
  Serial.println("Sensor inicializado\n");
}

void loop() {
  Serial.println("Leyendo sensor...");
  
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  
  if (isnan(h) || isnan(t)) {
    Serial.println("❌ ERROR: No se puede leer el sensor DHT11");
    Serial.println("Verifica:");
    Serial.println("  1. Conexiones (VCC->3.3V, DATA->GPIO4, GND->GND)");
    Serial.println("  2. Sensor funcionando");
    Serial.println("  3. Si es DHT22 en lugar de DHT11");
  } else {
    Serial.println("✓ LECTURA EXITOSA:");
    Serial.print("  Temperatura: ");
    Serial.print(t);
    Serial.println(" °C");
    Serial.print("  Humedad: ");
    Serial.print(h);
    Serial.println(" %");
  }
  
  Serial.println();
  delay(3000);
}
