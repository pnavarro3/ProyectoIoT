#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "yiyiyi";
const char* password = "xabicrack";

// Pines relés HW-316 (configuración actual)
const int RELE1_SUBIR_LENTO = 26;   // Subir lento (activo LOW)
const int RELE2_BAJAR_LENTO = 25;   // Bajar lento (activo LOW)
const int RELE3_RAPIDO = 27;        // Pasar a modo rapido (activo LOW)
const int RELE4_POWER = 14;         // Alimentación general (activo LOW)

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

// Modo de operación: true = manual, false = automático
bool modoManual = true;

WebServer server(80);

// Apagar todo (relés inactivos = HIGH)
void apagarTodo() {
  digitalWrite(RELE1_SUBIR_LENTO, HIGH);
  digitalWrite(RELE2_BAJAR_LENTO, HIGH);
  digitalWrite(RELE3_RAPIDO, HIGH);
  digitalWrite(RELE4_POWER, HIGH);
  powerOn = false;
  estadoMotor = "Parado";
  velocidad = "-";
  direccion = "-";
  destino = -1;
}

// Encender sistema (RELE4 activo = LOW)
void encenderSistema() {
  digitalWrite(RELE4_POWER, LOW);
  powerOn = true;
}

// Medir distancia ultrasónica
float medirDistancia() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  long duracion = pulseIn(ECHO, HIGH, 30000); // timeout 30ms
  if (duracion == 0) return distanciaActual; // si no hay eco, mantener última medida
  float distancia = duracion * 0.034 / 2; // cm
  return distancia;
}

// Funciones de control manual (activo = LOW)
// Nota: cuando se activa RELE3 (rápido), RELE1 o RELE2 se mantienen en el estado que indiquen la dirección.
// Es decir, no desactivamos RELE1/RELE2 al poner rápido.
void manualSubirLento() {
  if (!powerOn || !modoManual) return;
  destino = -1; // desactivar modo automático
  digitalWrite(RELE2_BAJAR_LENTO, HIGH);   // desactivar bajada
  digitalWrite(RELE1_SUBIR_LENTO, LOW);    // activar subir lento
  digitalWrite(RELE3_RAPIDO, HIGH);        // desactivar rápido
  estadoMotor = "Arrancado";
  direccion = "Subida";
  velocidad = "Lento";
}

void manualBajarLento() {
  if (!powerOn || !modoManual) return;
  destino = -1;
  digitalWrite(RELE1_SUBIR_LENTO, HIGH);   // desactivar subida
  digitalWrite(RELE2_BAJAR_LENTO, LOW);    // activar bajar lento
  digitalWrite(RELE3_RAPIDO, HIGH);        // desactivar rápido
  estadoMotor = "Arrancado";
  direccion = "Bajada";
  velocidad = "Lento";
}

void manualRapido() {
  if (!powerOn || !modoManual) return;
  // Activar rápido sin desactivar el relé de dirección (RELE1 o RELE2 se mantienen)
  if (direccion == "Subida" || direccion == "Bajada") {
    digitalWrite(RELE3_RAPIDO, LOW);        // activar rápido
    estadoMotor = "Arrancado";
    velocidad = "Rápido";
    // RELE1_SUBIR_LENTO o RELE2_BAJAR_LENTO se mantienen en su estado actual (LOW si estaban activos)
  } else {
    // Si no hay dirección definida, no activamos rápido por seguridad
  }
}

void manualParar() {
  if (!powerOn || !modoManual) return;
  digitalWrite(RELE1_SUBIR_LENTO, HIGH);
  digitalWrite(RELE2_BAJAR_LENTO, HIGH);
  digitalWrite(RELE3_RAPIDO, HIGH);
  estadoMotor = "Parado";
  velocidad = "-";
  direccion = "-";
  destino = -1;
}

// Mover hacia destino (automático)
// Al activar modo rápido (RELE3), mantenemos el relé de dirección activo (RELE1 o RELE2).
void moverAscensor() {
  if (destino < 0 || !powerOn || modoManual) return;

  distanciaActual = medirDistancia();
  float diferencia = destino - distanciaActual;

  if (abs(diferencia) <= 0.5) {
    // Llegó al destino: desactivar todos (relés inactivos = HIGH)
    digitalWrite(RELE1_SUBIR_LENTO, HIGH);
    digitalWrite(RELE2_BAJAR_LENTO, HIGH);
    digitalWrite(RELE3_RAPIDO, HIGH);
    estadoMotor = "Parado";
    velocidad = "-";
    direccion = "-";
    destino = -1;
    return;
  }

  // Dirección (activar relé correspondiente en LOW y mantenerlo)
  if (diferencia > 0) {
    direccion = "Subida";
    digitalWrite(RELE2_BAJAR_LENTO, HIGH);   // desactivar bajada
    digitalWrite(RELE1_SUBIR_LENTO, LOW);    // activar subida lenta (se mantiene)
  } else {
    direccion = "Bajada";
    digitalWrite(RELE1_SUBIR_LENTO, HIGH);   // desactivar subida
    digitalWrite(RELE2_BAJAR_LENTO, LOW);    // activar bajada lenta (se mantiene)
  }

  estadoMotor = "Arrancado";

  // Velocidad según tramo: primeros 3cm lento, intermedio rápido, últimos 3cm lento
  float distanciaRestante = abs(diferencia);
  // Simplificación: si distanciaRestante > 6 -> rápido; si <= 6 -> lento
  if (distanciaRestante > 6.0) {
    // Activar rápido sin desactivar el relé de dirección
    digitalWrite(RELE3_RAPIDO, LOW);   // activar rápido (LOW)
    velocidad = "Rápido";
  } else {
    digitalWrite(RELE3_RAPIDO, HIGH);  // desactivar rápido (HIGH) -> lento
    velocidad = "Lento";
  }
}

// Cambiar modo
void setModoManual() {
  modoManual = true;
  destino = -1;
  // dejar control manual al operador (no forzar apagado de relés)
}

void setModoAutomatico() {
  modoManual = false;
  // parar relés para que moverAscensor gestione el movimiento
  digitalWrite(RELE1_SUBIR_LENTO, HIGH);
  digitalWrite(RELE2_BAJAR_LENTO, HIGH);
  digitalWrite(RELE3_RAPIDO, HIGH);
  estadoMotor = "Parado";
  velocidad = "-";
  direccion = "-";
}

// Página web
String paginaHTML() {
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Montacargas Dahlander</title>";
  html += "<style>button{padding:10px 18px;margin:6px;font-size:16px;} .disabled{opacity:0.5;pointer-events:none;} .modo{font-weight:bold;margin-left:8px;}</style>";
  html += "<script>function actualizar(){fetch('/estado').then(r=>r.json()).then(d=>{";
  html += "document.getElementById('estado').innerText=d.estado;";
  html += "document.getElementById('direccion').innerText=d.direccion;";
  html += "document.getElementById('velocidad').innerText=d.velocidad;";
  html += "document.getElementById('distancia').innerText=d.distancia;";
  html += "document.getElementById('destino').innerText=d.destino;";
  html += "document.getElementById('power').innerText=d.power;";
  html += "document.getElementById('modo').innerText=d.modo;";
  html += "var plantabot=document.getElementsByClassName('plantabot'); var manualbot=document.getElementsByClassName('manualbot');";
  html += "for(var i=0;i<plantabot.length;i++){ if(d.power=='OFF' || d.modo!='AUTO'){plantabot[i].classList.add('disabled');} else {plantabot[i].classList.remove('disabled');}}";
  html += "for(var i=0;i<manualbot.length;i++){ if(d.power=='OFF' || d.modo!='MANUAL'){manualbot[i].classList.add('disabled');} else {manualbot[i].classList.remove('disabled');}}";
  html += "});}";
  html += "setInterval(actualizar,1000);</script></head><body>";
  html += "<h1>Control Montacargas Dahlander</h1>";
  html += "<p><b>Power:</b> <span id='power'>" + String(powerOn ? "ON" : "OFF") + "</span></p>";
  html += "<p><b>Modo:</b> <span id='modo'>" + String(modoManual ? "MANUAL" : "AUTO") + "</span></p>";
  html += "<p><b>Estado:</b> <span id='estado'>" + estadoMotor + "</span></p>";
  html += "<p><b>Dirección:</b> <span id='direccion'>" + direccion + "</span></p>";
  html += "<p><b>Velocidad:</b> <span id='velocidad'>" + velocidad + "</span></p>";
  html += "<p><b>Distancia actual:</b> <span id='distancia'>" + String(distanciaActual) + " cm</span></p>";
  html += "<p><b>Destino:</b> <span id='destino'>" + String(destino) + " cm</span></p>";

  // Botón Power independiente
  if (powerOn) {
    html += "<form action='/poweroff'><button>Apagar Sistema</button></form><hr>";
  } else {
    html += "<form action='/poweron'><button>Encender Sistema</button></form><hr>";
  }

  // Selección de modo
  html += "<div><form action='/modo_manual' style='display:inline'><button>Modo Manual</button></form>";
  html += "<form action='/modo_auto' style='display:inline'><button>Modo Automático</button></form></div><hr>";

  // Botones de plantas (solo en automático; clase plantabot)
  html += "<div><h3>Automático (pisos)</h3>";
  html += "<form action='/planta0' class='plantabot'><button>Planta Baja</button></form>";
  html += "<form action='/planta1' class='plantabot'><button>Planta 1</button></form>";
  html += "<form action='/planta2' class='plantabot'><button>Planta 2</button></form>";
  html += "<form action='/planta3' class='plantabot'><button>Planta 3</button></form></div><hr>";

  // Controles manuales (solo en manual; clase manualbot)
  html += "<div><h3>Manual</h3>";
  html += "<form action='/subir' class='manualbot'><button>Subir Lento</button></form>";
  html += "<form action='/bajar' class='manualbot'><button>Bajar Lento</button></form>";
  html += "<form action='/rapido' class='manualbot'><button>Rápido (mantener sentido)</button></form>";
  html += "<form action='/parar' class='manualbot'><button>Parar</button></form></div>";

  html += "</body></html>";
  return html;
}

// Rutas
void handleRoot() { server.send(200, "text/html", paginaHTML()); }
void handlePowerOn() { encenderSistema(); server.send(200, "text/html", paginaHTML()); }
void handlePowerOff() { apagarTodo(); server.send(200, "text/html", paginaHTML()); }

// Modo
void handleModoManual() { setModoManual(); server.send(200, "text/html", paginaHTML()); }
void handleModoAuto() { setModoAutomatico(); server.send(200, "text/html", paginaHTML()); }

// Plantas (automático)
void handlePlanta0() { if (powerOn && !modoManual) destino = 0; server.send(200, "text/html", paginaHTML()); }
void handlePlanta1() { if (powerOn && !modoManual) destino = 10; server.send(200, "text/html", paginaHTML()); }
void handlePlanta2() { if (powerOn && !modoManual) destino = 20; server.send(200, "text/html", paginaHTML()); }
void handlePlanta3() { if (powerOn && !modoManual) destino = 30; server.send(200, "text/html", paginaHTML()); }

// Controles manuales
void handleSubir() { if (powerOn && modoManual) manualSubirLento(); server.send(200, "text/html", paginaHTML()); }
void handleBajar() { if (powerOn && modoManual) manualBajarLento(); server.send(200, "text/html", paginaHTML()); }
void handleRapido() { if (powerOn && modoManual) manualRapido(); server.send(200, "text/html", paginaHTML()); }
void handleParar() { if (powerOn && modoManual) manualParar(); server.send(200, "text/html", paginaHTML()); }

void handleEstado() {
  String json = "{\"estado\":\"" + estadoMotor + "\",\"direccion\":\"" + direccion + "\",\"velocidad\":\"" + velocidad + "\",\"distancia\":\"" + String(distanciaActual) + "\",\"destino\":\"" + String(destino) + "\",\"power\":\"" + (powerOn ? "ON" : "OFF") + "\",\"modo\":\"" + (modoManual ? "MANUAL" : "AUTO") + "\"}";
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
  server.on("/modo_manual", handleModoManual);
  server.on("/modo_auto", handleModoAuto);
  server.on("/planta0", handlePlanta0);
  server.on("/planta1", handlePlanta1);
  server.on("/planta2", handlePlanta2);
  server.on("/planta3", handlePlanta3);
  server.on("/subir", handleSubir);
  server.on("/bajar", handleBajar);
  server.on("/rapido", handleRapido);
  server.on("/parar", handleParar);
  server.on("/estado", handleEstado);

  server.begin();
}

void loop() {
  server.handleClient();   // Atiende las peticiones web
  // Actualizar lectura de distancia periódicamente
  distanciaActual = medirDistancia();
  // Ejecutar lógica automática si hay destino y estamos en modo automático
  moverAscensor();
}
