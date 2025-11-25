#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "yiyiyi";
const char* password = "xabicrack";

// Pines relés HW-316
const int RELE1_SUBIR_LENTO = 5;   // Subir lento
const int RELE2_BAJAR_LENTO = 18;  // Bajar lento
const int RELE3_RAPIDO = 19;       // Pasar a modo rapido
const int RELE4_POWER = 21;        // Alimentación general

// Pines sensor ultrasónico
const int TRIG = 23;
const int ECHO = 22;

// Estado
float distanciaActual = 0;
float destino = -1;
String estadoMotor = "Parado";
String velocidad = "-";
String direccion = "-";
bool powerOn = false;

WebServer server(80);

// Apagar todo
void apagarTodo() {
  digitalWrite(RELE1_SUBIR_LENTO, LOW);
  digitalWrite(RELE2_BAJAR_LENTO, LOW);
  digitalWrite(RELE3_RAPIDO, LOW);
  digitalWrite(RELE4_POWER, LOW);
  powerOn = false;
  estadoMotor = "Parado";
  velocidad = "-";
  direccion = "-";
  destino = -1;
}

// Encender sistema
void encenderSistema() {
  digitalWrite(RELE4_POWER, HIGH);
  powerOn = true;
}

// Medir distancia ultrasónica
float medirDistancia() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  long duracion = pulseIn(ECHO, HIGH);
  float distancia = duracion * 0.034 / 2; // cm
  return distancia;
}

// Mover hacia destino
void moverAscensor() {
  if (destino < 0 || !powerOn) return;

  distanciaActual = medirDistancia();
  float diferencia = destino - distanciaActual;

  if (abs(diferencia) <= 0.5) {
    estadoMotor = "Parado";
    velocidad = "-";
    direccion = "-";
    digitalWrite(RELE1_SUBIR_LENTO, LOW);
    digitalWrite(RELE2_BAJAR_LENTO, LOW);
    digitalWrite(RELE3_RAPIDO, LOW);
    return;
  }

  // Dirección
  if (diferencia > 0) {
    direccion = "Subida";
    digitalWrite(RELE2_BAJAR_LENTO, LOW);
    digitalWrite(RELE1_SUBIR_LENTO, HIGH);
  } else {
    direccion = "Bajada";
    digitalWrite(RELE1_SUBIR_LENTO, LOW);
    digitalWrite(RELE2_BAJAR_LENTO, HIGH);
  }

  estadoMotor = "Arrancado";

  // Velocidad según tramo
  float distanciaRestante = abs(diferencia);
  if (distanciaRestante > 6) {
    digitalWrite(RELE3_RAPIDO, HIGH);
    velocidad = "Rápido";
  } else {
    digitalWrite(RELE3_RAPIDO, LOW);
    velocidad = "Lento";
  }
}

// Página web
String paginaHTML() {
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Montacargas Dahlander</title>";
  html += "<script>function actualizar(){fetch('/estado').then(r=>r.json()).then(d=>{";
  html += "document.getElementById('estado').innerText=d.estado;";
  html += "document.getElementById('direccion').innerText=d.direccion;";
  html += "document.getElementById('velocidad').innerText=d.velocidad;";
  html += "document.getElementById('distancia').innerText=d.distancia;";
  html += "document.getElementById('destino').innerText=d.destino;";
  html += "document.getElementById('power').innerText=d.power;});}";
  html += "setInterval(actualizar,1000);</script></head><body>";
  html += "<h1>Control Montacargas Dahlander</h1>";
  html += "<p><b>Power:</b> <span id='power'>" + String(powerOn ? "ON" : "OFF") + "</span></p>";
  html += "<p><b>Estado:</b> <span id='estado'>" + estadoMotor + "</span></p>";
  html += "<p><b>Dirección:</b> <span id='direccion'>" + direccion + "</span></p>";
  html += "<p><b>Velocidad:</b> <span id='velocidad'>" + velocidad + "</span></p>";
  html += "<p><b>Distancia actual:</b> <span id='distancia'>" + String(distanciaActual) + " cm</span></p>";
  html += "<p><b>Destino:</b> <span id='destino'>" + String(destino) + " cm</span></p>";

  // Botón Power independiente
  if (powerOn) {
    html += "<form action='/poweroff'><button>Apagar Sistema</button></form>";
    // Botones de plantas habilitados solo si powerOn
    html += "<form action='/planta0'><button>Planta Baja</button></form>";
    html += "<form action='/planta1'><button>Planta 1</button></form>";
    html += "<form action='/planta2'><button>Planta 2</button></form>";
    html += "<form action='/planta3'><button>Planta 3</button></form>";
    html += "<form action='/parar'><button>Parar</button></form>";
  } else {
    html += "<form action='/poweron'><button>Encender Sistema</button></form>";
    html += "<p><i>El sistema está apagado. Enciéndelo para mover el montacargas.</i></p>";
  }

  html += "</body></html>";
  return html;
}

// Rutas
void handleRoot() { server.send(200, "text/html", paginaHTML()); }
void handlePowerOn() { encenderSistema(); server.send(200, "text/html", paginaHTML()); }
void handlePowerOff() { apagarTodo(); server.send(200, "text/html", paginaHTML()); }
void handlePlanta0() { if (powerOn) destino = 0; server.send(200, "text/html", paginaHTML()); }
void handlePlanta1() { if (powerOn) destino = 10; server.send(200, "text/html", paginaHTML()); }
void handlePlanta2() { if (powerOn) destino = 20; server.send(200, "text/html", paginaHTML()); }
void handlePlanta3() { if (powerOn) destino = 30; server.send(200, "text/html", paginaHTML()); }
void handleParar() { if (powerOn) destino = -1; apagarTodo(); server.send(200, "text/html", paginaHTML()); }

void handleEstado() {
  String json = "{\"estado\":\"" + estadoMotor + "\",\"direccion\":\"" + direccion + "\",\"velocidad\":\"" + velocidad + "\",\"distancia\":\"" + String(distanciaActual) + "\",\"destino\":\"" + String(destino) + "\",\"power\":\"" + (powerOn ? "ON" : "OFF") + "\"}";
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  pinMode(RELE1_SUBIR_LENTO, OUTPUT);
  pinMode(RELE2_BAJAR_LENTO, OUTPUT);
  pinMode(RELE3_RAPIDO, OUTPUT);
  pinMode(RELE4_POWER, OUTPUT);
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  apagarTodo();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("Conectado a WiFi!");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/poweron", handlePowerOn);
  server.on("/poweroff", handlePowerOff);
  server.on("/planta0", handlePlanta0);
  server.on("/planta1", handlePlanta1);
  server.on("/planta2", handlePlanta2);
  server.on("/planta3", handlePlanta3);
  server.on("/parar", handleParar);
  server.on("/estado", handleEstado);

  server.begin();
}

void loop() {

