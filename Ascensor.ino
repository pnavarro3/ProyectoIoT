#include <WiFi.h>
#include <WebServer.h>

// ⚡ Configuración WiFi
const char* ssid = "TU_SSID";
const char* password = "TU_PASSWORD";

// ⚡ Pines de relés
const int RELE_ESTRELLA = 5;
const int RELE_TRIANGULO = 18;
const int RELE_DERECHA = 19;
const int RELE_IZQUIERDA = 21;

// ⚡ Variables de estado
String estadoMotor = "Parado";
String estadoArranque = "-";
String sentidoGiro = "-";

WebServer server(80);

// 🛑 Apagar todo
void apagarTodo() {
  digitalWrite(RELE_ESTRELLA, LOW);
  digitalWrite(RELE_TRIANGULO, LOW);
  digitalWrite(RELE_DERECHA, LOW);
  digitalWrite(RELE_IZQUIERDA, LOW);
  estadoMotor = "Parado";
  estadoArranque = "-";
  sentidoGiro = "-";
}

// 🚀 Arrancar motor
void arrancarMotor(bool derecha) {
  apagarTodo(); // seguridad

  // Sentido de giro
  if (derecha) {
    digitalWrite(RELE_DERECHA, HIGH);
    sentidoGiro = "Derecha";
  } else {
    digitalWrite(RELE_IZQUIERDA, HIGH);
    sentidoGiro = "Izquierda";
  }

  // Arranque en estrella
  digitalWrite(RELE_ESTRELLA, HIGH);
  estadoArranque = "Estrella";
  estadoMotor = "Arrancado";

  delay(3000); // esperar 3 segundos

  // Conmutar a triángulo
  digitalWrite(RELE_ESTRELLA, LOW);
  digitalWrite(RELE_TRIANGULO, HIGH);
  estadoArranque = "Triángulo";
}

// 🌐 Página web
String paginaHTML() {
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Control Motor</title></head><body>";
  html += "<h1>Control Motor Estrella-Triángulo</h1>";
  html += "<p><b>Estado:</b> " + estadoMotor + "</p>";
  html += "<p><b>Sentido:</b> " + sentidoGiro + "</p>";
  html += "<p><b>Arranque:</b> " + estadoArranque + "</p>";
  html += "<form action='/derecha'><button>Arrancar Derecha</button></form>";
  html += "<form action='/izquierda'><button>Arrancar Izquierda</button></form>";
  html += "<form action='/parar'><button>Parar</button></form>";
  html += "</body></html>";
  return html;
}

// 📡 Rutas servidor
void handleRoot() {
  server.send(200, "text/html", paginaHTML());
}

void handleDerecha() {
  arrancarMotor(true);
  server.send(200, "text/html", paginaHTML());
}

void handleIzquierda() {
  arrancarMotor(false);
  server.send(200, "text/html", paginaHTML());
}

void handleParar() {
  apagarTodo();
  server.send(200, "text/html", paginaHTML());
}

void setup() {
  Serial.begin(115200);

  // Configurar pines
  pinMode(RELE_ESTRELLA, OUTPUT);
  pinMode(RELE_TRIANGULO, OUTPUT);
  pinMode(RELE_DERECHA, OUTPUT);
  pinMode(RELE_IZQUIERDA, OUTPUT);

  apagarTodo();

  // Conectar WiFi
  WiFi.begin(ssid, password);
  Serial.print("Conectando a WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Conectado!");
  Serial.println(WiFi.localIP());

  // Configurar servidor
  server.on("/", handleRoot);
  server.on("/derecha", handleDerecha);
  server.on("/izquierda", handleIzquierda);
  server.on("/parar", handleParar);

  server.begin();
}

void loop() {
  server.handleClient();
}
