/*
 * ESP32 - Control de Montacargas con MQTT y LCD
 * Recibe datos de sensores vía MQTT
 * Muestra estado en LCD I2C 16x2
 * Interfaz web de control
 */

#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <LiquidCrystal_I2C.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

const char* ssid = "yiyiyi";
const char* password = "xabicrack";

// ========== CONFIGURACIÓN MQTT ==========
// IMPORTANTE: Cambia esta IP por la de tu portátil cuando lo configures
// Para obtenerla, ejecuta en PowerShell: ipconfig
// Busca la IPv4 de tu adaptador WiFi/Ethernet (ejemplo: 192.168.1.XXX)
const char* mqtt_server = "192.168.1.100";  // ⚠️ CAMBIAR por IP de tu portátil
const int mqtt_port = 1883;
const char* mqtt_user = "";
const char* mqtt_password = "";
const char* mqtt_client_id = "ESP32_Montacargas";

// ========== CONFIGURACIÓN NEWS API ==========
// Obtén tu API Key gratis en: https://newsapi.org
const char* newsApiKey = "64cac7cb2c8b49b599852cb50d4e97dd";
const unsigned long intervaloActualizacionNoticias = 7200000; // 2 horas en milisegundos
unsigned long ultimaActualizacionNoticias = 0;
String noticiasCache = "[]"; // Cache de noticias en formato JSON

// Pines relés HW-316 (configuración actual)
const int RELE1_SUBIR_LENTO = 26;   // Subir lento (activo LOW)
const int RELE2_BAJAR_LENTO = 25;   // Bajar lento (activo LOW)
const int RELE3_RAPIDO = 27;        // Pasar a modo rapido (activo LOW)
const int RELE4_POWER = 14;         // Alimentación general (activo LOW)

// LED de iluminación
const int LED_LUZ = 2;              // LED que se enciende cuando está oscuro

// Definición de plantas según distancia del sensor (en cm)
const float PLANTA_0 = 5.0;   // Planta 0 a 5 cm
const float PLANTA_1 = 20.0;  // Planta 1 a 20 cm
const float PLANTA_2 = 35.0;  // Planta 2 a 35 cm
const float PLANTA_3 = 50.0;  // Planta 3 a 50 cm
const float TOLERANCIA = 1.0; // Margen de error para considerar llegada (±1cm)
const float DISTANCIA_LENTA = 4.0; // Primeros y últimos 4 cm en velocidad lenta

float distancias_plantas[4] = {PLANTA_0, PLANTA_1, PLANTA_2, PLANTA_3};

// Estado del montacargas
int plantaActual = 0;              // Planta donde está (0, 1, 2, 3)
int plantaDestino = -1;            // Destino (-1 = sin destino)
bool destinoAlcanzado = true;      // True cuando ya llegó al destino y no debe moverse
String estadoMotor = "Parado";
String velocidad = "-";
String direccion = "-";
bool powerOn = false;

// Datos de sensores (recibidos por MQTT)
float distanciaSensor = 0.0;
int luminosidad = 0;
float temperatura = 0.0;
float humedad = 0.0;
String estadoLuz = "Desconocido";

// Objetos
WiFiClient espClient;
PubSubClient mqttClient(espClient);
WebServer server(80);

// Pantalla LCD I2C (dirección 0x27, 16 columnas, 2 filas)
// Si no funciona con 0x27, prueba con 0x3F
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ========== FUNCIONES LCD ==========

// Actualizar pantalla LCD según estado
void actualizarLCD() {
  lcd.clear();
  
  if (!powerOn) {
    lcd.setCursor(0, 0);
    lcd.print("Sistema Apagado");
    return;
  }
  
  // Línea 1: Estado del ascensor
  lcd.setCursor(0, 0);
  if (direccion == "Subiendo") {
    lcd.print("  SUBIENDO...");
    lcd.setCursor(0, 1);
    lcd.print("Espere por favor");
  } 
  else if (direccion == "Bajando") {
    lcd.print("  BAJANDO...");
    lcd.setCursor(0, 1);
    lcd.print("Espere por favor");
  }
  else {
    // Parado - mostrar planta
    lcd.print("Planta: ");
    lcd.print(plantaActual);
    lcd.setCursor(0, 1);
    lcd.print("** DISPONIBLE **");
  }
}

// ========== FUNCIONES DE CONTROL ==========

// Determinar planta actual basándose en distancia del sensor
int determinarPlantaPorDistancia(float distancia) {
  int plantaMasCercana = 0;
  float menorDiferencia = abs(distancia - distancias_plantas[0]);
  
  for (int i = 1; i < 4; i++) {
    float diferencia = abs(distancia - distancias_plantas[i]);
    if (diferencia < menorDiferencia) {
      menorDiferencia = diferencia;
      plantaMasCercana = i;
    }
  }
  
  return plantaMasCercana;
}

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
  plantaDestino = -1;
  destinoAlcanzado = true;
  actualizarLCD();
}

// Encender sistema (RELE4 activo = LOW)
void encenderSistema() {
  digitalWrite(RELE4_POWER, LOW);
  powerOn = true;
  actualizarLCD();
}

// Callback MQTT - Recibir datos de sensores
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String mensaje = "";
  for (int i = 0; i < length; i++) {
    mensaje += (char)payload[i];
  }
  
  if (strcmp(topic, "montacargas/sensores/distancia") == 0) {
    distanciaSensor = mensaje.toFloat();
  }
  else if (strcmp(topic, "montacargas/sensores/luminosidad") == 0) {
    luminosidad = mensaje.toInt();
    // Valor digital: 1=Oscuro, 0=Claro
    estadoLuz = (luminosidad == 1) ? "Oscuro" : "Claro";
    
    // Controlar LED: Encender si está oscuro
    digitalWrite(LED_LUZ, (luminosidad == 1) ? HIGH : LOW);
  }
  else if (strcmp(topic, "montacargas/sensores/temperatura") == 0) {
    temperatura = mensaje.toFloat();
  }
  else if (strcmp(topic, "montacargas/sensores/humedad") == 0) {
    humedad = mensaje.toFloat();
  }
}

// Reconectar MQTT
void reconectarMQTT() {
  if (!mqttClient.connected()) {
    Serial.print("Conectando a MQTT...");
    if (mqttClient.connect(mqtt_client_id, mqtt_user, mqtt_password)) {
      Serial.println("conectado");
      // Suscribirse a los tópicos de sensores
      mqttClient.subscribe("montacargas/sensores/distancia");
      mqttClient.subscribe("montacargas/sensores/luminosidad");
      mqttClient.subscribe("montacargas/sensores/temperatura");
      mqttClient.subscribe("montacargas/sensores/humedad");
    } else {
      Serial.print("falló, rc=");
      Serial.println(mqttClient.state());
    }
  }
}

// Mover a una planta específica
void irAPlanta(int planta) {
  if (!powerOn || planta < 0 || planta > 3) return;
  
  // NO permitir cambios si está en movimiento
  if (plantaDestino != -1 && !destinoAlcanzado) {
    Serial.println("Montacargas en movimiento. Espere a que llegue al destino.");
    return;
  }
  
  // Validar que tenemos lectura del sensor
  if (distanciaSensor <= 0) {
    Serial.println("Esperando lectura del sensor para iniciar movimiento...");
    return;
  }
  
  // Verificar si ya está en la planta
  float distanciaDestino = distancias_plantas[planta];
  float diferencia = abs(distanciaSensor - distanciaDestino);
  
  if (diferencia <= TOLERANCIA) {
    // Ya está en la planta destino
    plantaActual = planta;
    plantaDestino = -1;
    destinoAlcanzado = true;
    detenerMontacargas();
    return;
  }
  
  // Nueva orden: resetear flag y establecer destino
  plantaDestino = planta;
  destinoAlcanzado = false;  // Permitir movimiento
  estadoMotor = "Arrancado";
  actualizarLCD();
}

void detenerMontacargas() {
  digitalWrite(RELE1_SUBIR_LENTO, HIGH);
  digitalWrite(RELE2_BAJAR_LENTO, HIGH);
  digitalWrite(RELE3_RAPIDO, HIGH);
  estadoMotor = "Parado";
  velocidad = "-";
  direccion = "-";
  if (plantaDestino != -1) {
    plantaActual = plantaDestino;
    plantaDestino = -1;
    destinoAlcanzado = true;  // Marcar que llegó al destino
  }
  actualizarLCD();
}

// Control de movimiento basado en distancia real del sensor
void actualizarPosicion() {
  if (plantaDestino == -1 || !powerOn || destinoAlcanzado) return;
  
  // Validar que tenemos lectura válida del sensor
  if (distanciaSensor <= 0) {
    Serial.println("Esperando lectura válida del sensor...");
    return;
  }
  
  // Obtener distancia actual y destino
  float distanciaDestino = distancias_plantas[plantaDestino];
  float diferencia = distanciaDestino - distanciaSensor;
  float distanciaRestante = abs(diferencia);
  
  // Verificar si ha llegado al destino
  if (distanciaRestante <= TOLERANCIA) {
    plantaActual = plantaDestino;
    detenerMontacargas();
    Serial.println("Destino alcanzado. Esperando nueva orden.");
    return;
  }
  
  // Determinar dirección
  if (diferencia > TOLERANCIA) {
    // Necesita subir
    if (direccion != "Subiendo") {
      direccion = "Subiendo";
      digitalWrite(RELE2_BAJAR_LENTO, HIGH);  // Desactivar bajada
      digitalWrite(RELE1_SUBIR_LENTO, LOW);   // Activar subida
      actualizarLCD();
    }
  } 
  else if (diferencia < -TOLERANCIA) {
    // Necesita bajar
    if (direccion != "Bajando") {
      direccion = "Bajando";
      digitalWrite(RELE1_SUBIR_LENTO, HIGH);  // Desactivar subida
      digitalWrite(RELE2_BAJAR_LENTO, LOW);   // Activar bajada
      actualizarLCD();
    }
  }
  
  // Control de velocidad según distancia recorrida y restante
  // Calcular distancia desde el origen
  float distanciaOrigen = distancias_plantas[plantaActual];
  float distanciaRecorrida = abs(distanciaSensor - distanciaOrigen);
  
  // Lógica: Primeros 4cm LENTO, últimos 4cm LENTO, resto RÁPIDO
  bool enZonaInicial = (distanciaRecorrida <= DISTANCIA_LENTA);
  bool enZonaFinal = (distanciaRestante <= DISTANCIA_LENTA);
  
  if (enZonaInicial || enZonaFinal) {
    // Modo LENTO
    if (velocidad != "Lento") {
      digitalWrite(RELE3_RAPIDO, HIGH);  // Desactivar rápido
      velocidad = "Lento";
      Serial.print("Velocidad: LENTO (Dist.Rest: ");
      Serial.print(distanciaRestante);
      Serial.println(" cm)");
    }
  } else {
    // Modo RÁPIDO
    if (velocidad != "Rápido") {
      digitalWrite(RELE3_RAPIDO, LOW);   // Activar rápido
      velocidad = "Rápido";
      Serial.print("Velocidad: RÁPIDO (Dist.Rest: ");
      Serial.print(distanciaRestante);
      Serial.println(" cm)");
    }
  }
}

// ========== PÁGINA WEB ==========

String paginaHTML() {
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Montacargas IoT</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<style>";
  html += "body{font-family:Arial,sans-serif;max-width:800px;margin:20px auto;padding:20px;background:#f5f5f5;}";
  html += "h1{color:#333;text-align:center;border-bottom:3px solid #4CAF50;padding-bottom:10px;}";
  html += ".info-grid{display:grid;grid-template-columns:repeat(2,1fr);gap:15px;margin:20px 0;}";
  html += ".info-card{background:white;padding:15px;border-radius:8px;box-shadow:0 2px 5px rgba(0,0,0,0.1);}";
  html += ".info-card h3{margin:0 0 10px 0;color:#4CAF50;font-size:16px;}";
  html += ".info-card .value{font-size:24px;font-weight:bold;color:#333;}";
  html += ".btn-grid{display:grid;grid-template-columns:repeat(2,1fr);gap:10px;margin:20px 0;}";
  html += "button{padding:15px;font-size:16px;border:none;border-radius:6px;cursor:pointer;background:#4CAF50;color:white;transition:0.3s;}";
  html += "button:hover{background:#45a049;transform:scale(1.02);}";
  html += "button:active{transform:scale(0.98);}";
  html += ".disabled{opacity:0.5;pointer-events:none;background:#ccc;}";
  html += ".system-btn{background:#2196F3;margin-bottom:20px;width:100%;}";
  html += ".system-btn.off{background:#f44336;}";
  html += ".planta-actual{font-size:48px;text-align:center;color:#4CAF50;font-weight:bold;margin:20px 0;}";
  html += ".aviso{background:#ff9800;color:white;padding:15px;border-radius:6px;text-align:center;font-weight:bold;margin:20px 0;display:none;}";
  html += ".aviso.show{display:block;animation:pulse 1.5s infinite;}";
  html += "@keyframes pulse{0%,100%{opacity:1;}50%{opacity:0.7;}}";
  html += ".noticias-section{margin-top:40px;background:white;padding:20px;border-radius:8px;box-shadow:0 2px 5px rgba(0,0,0,0.1);}";
  html += ".noticias-section h2{color:#2196F3;margin-top:0;border-bottom:2px solid #2196F3;padding-bottom:10px;}";
  html += ".noticia{border-left:4px solid #4CAF50;padding:15px;margin:15px 0;background:#f9f9f9;border-radius:4px;}";
  html += ".noticia h4{margin:0 0 8px 0;color:#333;font-size:16px;}";
  html += ".noticia p{margin:8px 0;color:#666;font-size:14px;line-height:1.4;}";
  html += ".noticia .fuente{color:#2196F3;font-size:12px;font-weight:bold;margin-top:8px;}";
  html += ".noticia a{color:#4CAF50;text-decoration:none;font-size:13px;}";
  html += ".noticia a:hover{text-decoration:underline;}";
  html += "@media(max-width:600px){.info-grid,.btn-grid{grid-template-columns:1fr;}}";
  html += "</style>";
  html += "<script>";
  html += "function actualizar(){fetch('/estado').then(r=>r.json()).then(d=>{";
  html += "document.getElementById('planta').innerText='Planta '+d.planta;";
  html += "document.getElementById('temperatura').innerText=d.temperatura+' °C';";
  html += "document.getElementById('humedad').innerText=d.humedad+' %';";
  html += "document.getElementById('luz').innerText=d.luz;";
  html += "document.getElementById('estado').innerText=d.estado;";
  html += "var aviso=document.getElementById('aviso');";
  html += "var btns=document.getElementsByClassName('plantabot');";
  html += "if(d.power=='OFF' || d.estado=='Subiendo' || d.estado=='Bajando'){";
  html += "for(var i=0;i<btns.length;i++){btns[i].classList.add('disabled');}";
  html += "if(d.estado=='Subiendo' || d.estado=='Bajando'){aviso.classList.add('show');}else{aviso.classList.remove('show');}";
  html += "}else{for(var i=0;i<btns.length;i++){btns[i].classList.remove('disabled');}aviso.classList.remove('show');}";
  html += "var sysBtn=document.getElementById('sysBtn');";
  html += "if(d.power=='ON'){sysBtn.innerText='🔌 SISTEMA ENCENDIDO - Clic para APAGAR';sysBtn.classList.remove('off');}";
  html += "else{sysBtn.innerText='⚡ SISTEMA APAGADO - Clic para ENCENDER';sysBtn.classList.add('off');}";
  html += "});}";
  html += "setInterval(actualizar,1000);";
  html += "function toggleSistema(){fetch('/togglepower').then(()=>actualizar());}";
  html += "function cargarNoticias(){fetch('/noticias').then(r=>r.json()).then(d=>{";
  html += "var container=document.getElementById('noticias-container');";
  html += "if(!d.noticias || d.noticias.length==0){container.innerHTML='<p>No hay noticias disponibles</p>';return;}";
  html += "container.innerHTML='';";
  html += "d.noticias.forEach(n=>{";
  html += "var div=document.createElement('div');div.className='noticia';";
  html += "div.innerHTML='<h4>'+n.titulo+'</h4><p>'+n.descripcion+'</p><div class=\"fuente\">📰 '+n.fuente+'</div><a href=\"'+n.url+'\" target=\"_blank\">Leer más →</a>';";
  html += "container.appendChild(div);";
  html += "});}).catch(e=>console.error('Error cargando noticias:',e));}";
  html += "window.onload=()=>{actualizar();cargarNoticias();};";
  html += "</script></head><body>";
  html += "<h1>🏢 Control Montacargas IoT</h1>";
  
  // Botón de sistema ON/OFF
  String btnTexto = powerOn ? "🔌 SISTEMA ENCENDIDO - Clic para APAGAR" : "⚡ SISTEMA APAGADO - Clic para ENCENDER";
  String btnClase = powerOn ? "system-btn" : "system-btn off";
  html += "<button id='sysBtn' class='" + btnClase + "' onclick='toggleSistema()'>" + btnTexto + "</button>";
  
  // Aviso de espera
  html += "<div id='aviso' class='aviso'>⚠️ MONTACARGAS EN MOVIMIENTO - ESPERE A QUE LLEGUE AL DESTINO</div>";
  
  // Planta actual grande
  html += "<div class='planta-actual' id='planta'>Planta " + String(plantaActual) + "</div>";
  
  // Grid de información
  html += "<div class='info-grid'>";
  html += "<div class='info-card'><h3>🌡️ Temperatura</h3><div class='value' id='temperatura'>" + String(temperatura, 1) + " °C</div></div>";
  html += "<div class='info-card'><h3>💧 Humedad</h3><div class='value' id='humedad'>" + String(humedad, 1) + " %</div></div>";
  html += "<div class='info-card'><h3>💡 Iluminación</h3><div class='value' id='luz'>" + estadoLuz + "</div></div>";
  html += "<div class='info-card'><h3>🔄 Estado</h3><div class='value' id='estado'>" + direccion + "</div></div>";
  html += "</div>";
  
  // Botones de plantas
  html += "<h3 style='margin-top:30px;text-align:center;'>Seleccionar Planta</h3>";
  html += "<div class='btn-grid'>";
  html += "<button class='plantabot' onclick=\"location.href='/planta0'\">Planta 0</button>";
  html += "<button class='plantabot' onclick=\"location.href='/planta1'\">Planta 1</button>";
  html += "<button class='plantabot' onclick=\"location.href='/planta2'\">Planta 2</button>";
  html += "<button class='plantabot' onclick=\"location.href='/planta3'\">Planta 3</button>";
  html += "</div>";
  
  // Widget de noticias IoT
  html += "<div class='noticias-section'><h2>📰 Últimas Noticias de IoT</h2>";
  html += "<div id='noticias-container'><p>Cargando noticias...</p></div></div>";
  
  html += "</body></html>";
  return html;
}

// ========== FUNCIONES NEWS API ==========

// Obtener noticias de IoT desde News API
void obtenerNoticiasIoT() {
  // Solo actualizar si ha pasado el intervalo (2 horas) o es la primera vez
  if (millis() - ultimaActualizacionNoticias < intervaloActualizacionNoticias && noticiasCache != "[]") {
    Serial.println("Usando noticias en caché");
    return;
  }
  
  Serial.println("Obteniendo noticias de IoT desde News API...");
  
  HTTPClient http;
  
  // Búsqueda de noticias sobre IoT, Internet of Things, automatización
  String url = "https://newsapi.org/v2/everything?q=IoT+OR+\"Internet+of+Things\"+OR+automation&language=es&sortBy=publishedAt&pageSize=5&apiKey=";
  url += newsApiKey;
  
  http.begin(url);
  http.setTimeout(10000); // Timeout de 10 segundos
  
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    String payload = http.getString();
    
    // Parsear JSON y crear versión simplificada
    DynamicJsonDocument doc(8192);
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      JsonArray articulos = doc["articles"];
      
      // Crear JSON simplificado solo con lo necesario
      DynamicJsonDocument noticiasDoc(4096);
      JsonArray noticiasArray = noticiasDoc.createNestedArray("noticias");
      
      for (JsonObject articulo : articulos) {
        JsonObject noticia = noticiasArray.createNestedObject();
        noticia["titulo"] = articulo["title"].as<String>();
        noticia["descripcion"] = articulo["description"].as<String>();
        noticia["fuente"] = articulo["source"]["name"].as<String>();
        noticia["url"] = articulo["url"].as<String>();
        noticia["fecha"] = articulo["publishedAt"].as<String>();
      }
      
      // Convertir a string JSON
      noticiasCache = "";
      serializeJson(noticiasDoc, noticiasCache);
      
      ultimaActualizacionNoticias = millis();
      Serial.println("✓ Noticias actualizadas correctamente");
      Serial.print("Total noticias: ");
      Serial.println(noticiasArray.size());
    } else {
      Serial.print("Error parseando JSON: ");
      Serial.println(error.c_str());
    }
  } else {
    Serial.print("Error HTTP: ");
    Serial.println(httpCode);
    if (httpCode == 401) {
      Serial.println("⚠️ Error 401: Verifica tu API Key de News API");
    } else if (httpCode == 429) {
      Serial.println("⚠️ Error 429: Límite de peticiones alcanzado");
    }
  }
  
  http.end();
}

// ========== HANDLERS WEB ==========

void handleRoot() { 
  server.send(200, "text/html", paginaHTML()); 
}

void handlePowerOn() { 
  encenderSistema(); 
  server.send(200, "text/html", paginaHTML()); 
}

void handlePowerOff() { 
  apagarTodo(); 
  server.send(200, "text/html", paginaHTML()); 
}

void handleTogglePower() {
  if (powerOn) {
    apagarTodo();
  } else {
    encenderSistema();
  }
  server.send(200, "text/plain", "OK");
}

void handlePlanta0() { 
  irAPlanta(0); 
  server.send(200, "text/html", paginaHTML()); 
}

void handlePlanta1() { 
  irAPlanta(1); 
  server.send(200, "text/html", paginaHTML()); 
}

void handlePlanta2() { 
  irAPlanta(2); 
  server.send(200, "text/html", paginaHTML()); 
}

void handlePlanta3() { 
  irAPlanta(3); 
  server.send(200, "text/html", paginaHTML()); 
}

void handleEstado() {
  String json = "{";
  json += "\"planta\":" + String(plantaActual) + ",";
  json += "\"temperatura\":" + String(temperatura, 1) + ",";
  json += "\"humedad\":" + String(humedad, 1) + ",";
  json += "\"luz\":\"" + estadoLuz + "\",";
  json += "\"estado\":\"" + direccion + "\",";
  json += "\"power\":\"" + (powerOn ? "ON" : "OFF") + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleNoticias() {
  // Servir noticias desde caché
  server.send(200, "application/json", noticiasCache);
}

// ========== SETUP Y LOOP ==========

void setup() {
  Serial.begin(115200);
  
  // Inicializar LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Iniciando...");
  lcd.setCursor(0, 1);
  lcd.print("Montacargas IoT");
  delay(2000);
  
  // Configurar pines de relés
  pinMode(RELE1_SUBIR_LENTO, OUTPUT);
  pinMode(RELE2_BAJAR_LENTO, OUTPUT);
  pinMode(RELE3_RAPIDO, OUTPUT);
  pinMode(RELE4_POWER, OUTPUT);
  
  // Configurar pin LED
  pinMode(LED_LUZ, OUTPUT);
  digitalWrite(LED_LUZ, LOW);  // Apagado inicialmente
  
  apagarTodo();

  // Conectar a WiFi
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Conectando WiFi");
  Serial.print("Conectando a WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { 
    delay(500); 
    Serial.print("."); 
  }
  Serial.println("\nConectado a WiFi!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi OK");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());
  delay(2000);

  // Configurar MQTT
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);

  // Configurar rutas web
  server.on("/", handleRoot);
  server.on("/poweron", handlePowerOn);
  server.on("/poweroff", handlePowerOff);
  server.on("/togglepower", handleTogglePower);
  server.on("/planta0", handlePlanta0);
  server.on("/planta1", handlePlanta1);
  server.on("/planta2", handlePlanta2);
  server.on("/planta3", handlePlanta3);
  server.on("/estado", handleEstado);
  server.on("/noticias", handleNoticias);

  server.begin();
  Serial.println("Servidor web iniciado");
  
  // Obtener noticias iniciales de IoT
  Serial.println("\n========== Obteniendo noticias de IoT ==========");
  obtenerNoticiasIoT();
  Serial.println("=================================================\n");
  
  // Esperar a recibir primera lectura del sensor
  Serial.println("Esperando datos de sensores...");
  
  // Mostrar estado inicial
  actualizarLCD();
}

void loop() {
  // Mantener conexión MQTT
  if (!mqttClient.connected()) {
    reconectarMQTT();
  }
  mqttClient.loop();
  
  // Atender peticiones web
  server.handleClient();
  
  // Actualizar noticias cada 2 horas
  static unsigned long ultimoCheckNoticias = 0;
  if (millis() - ultimoCheckNoticias > 60000) { // Verificar cada minuto si toca actualizar
    ultimoCheckNoticias = millis();
    if (millis() - ultimaActualizacionNoticias >= intervaloActualizacionNoticias) {
      Serial.println("⏰ Actualizando noticias de IoT...");
      obtenerNoticiasIoT();
    }
  }
  
  // Si no hay destino y está estable, actualizar planta actual según sensor
  if (plantaDestino == -1 && destinoAlcanzado && powerOn && distanciaSensor > 0) {
    int plantaDetectada = determinarPlantaPorDistancia(distanciaSensor);
    // Solo actualizar si la diferencia es significativa (más de tolerancia)
    float distPlantaActual = abs(distanciaSensor - distancias_plantas[plantaActual]);
    float distPlantaDetectada = abs(distanciaSensor - distancias_plantas[plantaDetectada]);
    
    if (plantaDetectada != plantaActual && distPlantaDetectada < distPlantaActual) {
      plantaActual = plantaDetectada;
      Serial.print("Planta actualizada a: ");
      Serial.println(plantaActual);
      actualizarLCD();
    }
  }
  
  // Actualizar movimiento si hay destino
  actualizarPosicion();
}
