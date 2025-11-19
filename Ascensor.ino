#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "yiyiyi";
const char* password = "xabicrack";

const int RELE_ESTRELLA = 5;
const int RELE_TRIANGULO = 18;
const int RELE_DERECHA = 19;
const int RELE_IZQUIERDA = 21;

String estadoMotor = "Parado";
String estadoArranque = "-";
String sentidoGiro = "-";

WebServer server(80);

void apagarTodo() {
  digitalWrite(RELE_ESTRELLA, LOW);
  digitalWrite(RELE_TRIANGULO, LOW);
  digitalWrite(RELE_DERECHA, LOW);
  digitalWrite(RELE_IZQUIERDA, LOW);
  estadoMotor = "Parado";
  estadoArranque = "-";
  sentidoGiro = "-";
}

void arrancarMotor(bool derecha) {
  apagarTodo();
  if (derecha) {
    digitalWrite(RELE_DERECHA, HIGH);
    sentidoGiro = "Derecha";
  } else {
    digitalWrite(RELE_IZQUIERDA, HIGH);
    sentidoGiro = "Izquierda";
  }
  digitalWrite(RELE_ESTRELLA, HIGH);
  estadoArranque = "Estrella";
  estadoMotor = "Arrancado";
  delay(3000);
  digitalWrite(RELE_ESTRELLA, LOW);
  digitalWrite(RELE_TRIANGULO, HIGH);
  estadoArranque = "Triángulo";
}

// Página principal con AJAX
String paginaHTML() {
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Control Motor</title>";
  html += "<script>";
  html += "function actualizar(){fetch('/estado').then(r=>r.json()).then(d=>{";
  html += "document.getElementById('estado').innerText=d.estado;";
  html += "document.getElementById('sentido').innerText=d.sentido;";
  html += "document.getElementById('arranque').innerText=d.arranque;});}";
  html += "setInterval(actualizar,1000);"; // cada 1 segundo
  html += "</script></head><body>";
  html += "<h1>Control Motor Estrella-Triángulo</h1>";
  html += "<p><b>Estado:</b> <span id='estado'>" + estadoMotor + "</span></p>";
  html += "<p><b>Sentido:</b> <span id='sentido'>" + sentidoGiro + "</span></p>";
  html += "<p><b>Arranque:</b> <span id='arranque'>" + estadoArranque + "</span></p>";
  html += "<form action='/derecha'><button>Arrancar Derecha</button></form>";
  html += "<form action='/izquierda'><button>Arrancar Izquierda</button></form>";
  html += "<form action='/parar'><button>Parar</button></form>";
  html += "</body></html>";
  return html;
}

// Rutas
void handleRoot() { server.send(200, "text/html", paginaHTML()); }
void handleDerecha() { arrancarMotor(true); server.send(200, "text/html", paginaHTML()); }
void handleIzquierda() { arrancarMotor(false); server.send(200, "text/html", paginaHTML()); }
void handleParar() { apagarTodo(); server.send(200, "text/html", paginaHTML()); }

// Endpoint JSON para AJAX
void handleEstado() {
  String json = "{\"estado\":\"" + estadoMotor + "\",\"sentido\":\"" + sentidoGiro + "\",\"arranque\":\"" + estadoArranque + "\"}";
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  pinMode(RELE_ESTRELLA, OUTPUT);
  pinMode(RELE_TRIANGULO, OUTPUT);
  pinMode(RELE_DERECHA, OUTPUT);
  pinMode(RELE_IZQUIERDA, OUTPUT);
  apagarTodo();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("Conectado a WiFi!");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/derecha", handleDerecha);
  server.on("/izquierda", handleIzquierda);
  server.on("/parar", handleParar);
  server.on("/estado", handleEstado);

  server.begin();
}

void loop() { server.handleClient(); }
