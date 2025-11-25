#include <WiFi.h>
#include <WebServer.h>

// Configuración WiFi
const char* ssid = "yiyiyi";
const char* password = "xabicrack";

// Pines de relés HW-316
const int RELE1_SUBIR_LENTO = 5;   // Subir lento
const int RELE2_BAJAR_LENTO = 18;  // Bajar lento
const int RELE3_RAPIDO = 19;       // Pasar a velocidad rapida
const int RELE4_POWER = 21;        // Alimentación general

// Estado del sistema
String estadoMotor = "Parado";
String velocidad = "-";
String direccion = "-";
bool powerOn = false;

WebServer server(80);

void apagarTodo() {
  digitalWrite(RELE1_SUBIR_LENTO, LOW);
  digitalWrite(RELE2_BAJAR_LENTO, LOW);
  digitalWrite(RELE3_RAPIDO, LOW);
  digitalWrite(RELE4_POWER, LOW);
  powerOn = false;
  estadoMotor = "Parado";
  velocidad = "-";
  direccion = "-";
}

void encenderSistema() {
  digitalWrite(RELE4_POWER, HIGH);
  powerOn = true;
}

void subirLento() {
  if (!powerOn) return;
  digitalWrite(RELE1_SUBIR_LENTO, HIGH);
  digitalWrite(RELE2_BAJAR_LENTO, LOW);
  estadoMotor = "Arrancado";
  direccion = "Subida";
  velocidad = "Lento";
}

void bajarLento() {
  if (!powerOn) return;
  digitalWrite(RELE2_BAJAR_LENTO, HIGH);
  digitalWrite(RELE1_SUBIR_LENTO, LOW);
  estadoMotor = "Arrancado";
  direccion = "Bajada";
  velocidad = "Lento";
}

void modoRapido() {
  if (!powerOn) return;
  digitalWrite(RELE3_RAPIDO, HIGH);
  digitalWrite(RELE1_SUBIR_LENTO, LOW);
  digitalWrite(RELE2_BAJAR_LENTO, LOW);
  estadoMotor = "Arrancado";
  velocidad = "Rápido";
}

String paginaHTML() {
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Ascensor Dahlander</title>";
  html += "<script>function actualizar(){fetch('/estado').then(r=>r.json()).then(d=>{";
  html += "document.getElementById('estado').innerText=d.estado;";
  html += "document.getElementById('direccion').innerText=d.direccion;";
  html += "document.getElementById('velocidad').innerText=d.velocidad;";
  html += "document.getElementById('power').innerText=d.power;});}";
  html += "setInterval(actualizar,1000);</script></head><body>";
  html += "<h1>Control Ascensor Dahlander</h1>";
  html += "<p><b>Power:</b> <span id='power'>" + String(powerOn ? "ON" : "OFF") + "</span></p>";
  html += "<p><b>Estado:</b> <span id='estado'>" + estadoMotor + "</span></p>";
  html += "<p><b>Dirección:</b> <span id='direccion'>" + direccion + "</span></p>";
  html += "<p><b>Velocidad:</b> <span id='velocidad'>" + velocidad + "</span></p>";
  html += "<form action='/power'><button>Encender Sistema</button></form>";
  html += "<form action='/subir'><button>Subir Lento</button></form>";
  html += "<form action='/bajar'><button>Bajar Lento</button></form>";
  html += "<form action='/rapido'><button>Modo Rápido</button></form>";
  html += "<form action='/parar'><button>Parar</button></form>";
  html += "</body></html>";
  return html;
}

void handleRoot() { server.send(200, "text/html", paginaHTML()); }
void handlePower() { encenderSistema(); server.send(200, "text/html", paginaHTML()); }
void handleSubir() { subirLento(); server.send(200, "text/html", paginaHTML()); }
void handleBajar() { bajarLento(); server.send(200, "text/html", paginaHTML()); }
void handleRapido() { modoRapido(); server.send(200, "text/html", paginaHTML()); }
void handleParar() { apagarTodo(); server.send(200, "text/html", paginaHTML()); }

void handleEstado() {
  String json = "{\"estado\":\"" + estadoMotor + "\",\"direccion\":\"" + direccion + "\",\"velocidad\":\"" + velocidad + "\",\"power\":\"" + (powerOn ? "ON" : "OFF") + "\"}";
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  pinMode(RELE1_SUBIR_LENTO, OUTPUT);
  pinMode(RELE2_BAJAR_LENTO, OUTPUT);
  pinMode(RELE3_RAPIDO, OUTPUT);
  pinMode(RELE4_POWER, OUTPUT);
  apagarTodo();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("Conectado a WiFi!");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/power", handlePower);
  server.on("/subir", handleSubir);
  server.on("/bajar", handleBajar);
  server.on("/rapido", handleRapido);
  server.on("/parar", handleParar);
  server.on("/estado", handleEstado);

  server.begin();
}

void loop() { server.handleClient(); }
