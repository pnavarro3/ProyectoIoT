#include "Ascensor.h"

// Constructor
Ascensor::Ascensor(int rele1, int rele2, int rele3, int rele4, int trig, int echo) {
  pinSubirLento = rele1;
  pinBajarLento = rele2;
  pinRapido = rele3;
  pinPower = rele4;
  pinTrig = trig;
  pinEcho = echo;
  
  distanciaActual = 0;
  destino = -1;
  estadoMotor = "Parado";
  velocidad = "-";
  direccion = "-";
  powerOn = false;
  modoManual = true;
}

// Inicializar pines
void Ascensor::inicializar() {
  pinMode(pinSubirLento, OUTPUT);
  pinMode(pinBajarLento, OUTPUT);
  pinMode(pinRapido, OUTPUT);
  pinMode(pinPower, OUTPUT);
  pinMode(pinTrig, OUTPUT);
  pinMode(pinEcho, INPUT);
  
  desactivarReles();
}

// Desactivar todos los relés (HIGH = inactivo)
void Ascensor::desactivarReles() {
  digitalWrite(pinSubirLento, HIGH);
  digitalWrite(pinBajarLento, HIGH);
  digitalWrite(pinRapido, HIGH);
  digitalWrite(pinPower, HIGH);
  powerOn = false;
  estadoMotor = "Parado";
  velocidad = "-";
  direccion = "-";
  destino = -1;
}

// Encender sistema
void Ascensor::encender() {
  digitalWrite(pinPower, LOW);
  powerOn = true;
}

// Apagar sistema
void Ascensor::apagar() {
  desactivarReles();
}

bool Ascensor::estaEncendido() {
  return powerOn;
}

// Cambiar a modo manual
void Ascensor::setModoManual() {
  modoManual = true;
  destino = -1;
}

// Cambiar a modo automático
void Ascensor::setModoAutomatico() {
  modoManual = false;
  digitalWrite(pinSubirLento, HIGH);
  digitalWrite(pinBajarLento, HIGH);
  digitalWrite(pinRapido, HIGH);
  estadoMotor = "Parado";
  velocidad = "-";
  direccion = "-";
}

bool Ascensor::esModoManual() {
  return modoManual;
}

// Subir lento (manual)
void Ascensor::subirLento() {
  if (!powerOn || !modoManual) return;
  destino = -1;
  digitalWrite(pinBajarLento, HIGH);
  digitalWrite(pinSubirLento, LOW);
  digitalWrite(pinRapido, HIGH);
  estadoMotor = "Arrancado";
  direccion = "Subida";
  velocidad = "Lento";
}

// Bajar lento (manual)
void Ascensor::bajarLento() {
  if (!powerOn || !modoManual) return;
  destino = -1;
  digitalWrite(pinSubirLento, HIGH);
  digitalWrite(pinBajarLento, LOW);
  digitalWrite(pinRapido, HIGH);
  estadoMotor = "Arrancado";
  direccion = "Bajada";
  velocidad = "Lento";
}

// Activar modo rápido (manual)
void Ascensor::activarRapido() {
  if (!powerOn || !modoManual) return;
  if (direccion == "Subida" || direccion == "Bajada") {
    digitalWrite(pinRapido, LOW);
    estadoMotor = "Arrancado";
    velocidad = "Rápido";
  }
}

// Parar motor (manual)
void Ascensor::parar() {
  if (!powerOn || !modoManual) return;
  digitalWrite(pinSubirLento, HIGH);
  digitalWrite(pinBajarLento, HIGH);
  digitalWrite(pinRapido, HIGH);
  estadoMotor = "Parado";
  velocidad = "-";
  direccion = "-";
  destino = -1;
}

// Ir a planta (automático)
void Ascensor::irAPlanta(float distanciaDestino) {
  if (!powerOn || modoManual) return;
  destino = distanciaDestino;
}

// Medir distancia con sensor ultrasónico
float Ascensor::medirDistancia() {
  digitalWrite(pinTrig, LOW);
  delayMicroseconds(2);
  digitalWrite(pinTrig, HIGH);
  delayMicroseconds(10);
  digitalWrite(pinTrig, LOW);
  long duracion = pulseIn(pinEcho, HIGH, 30000);
  if (duracion == 0) return distanciaActual;
  float distancia = duracion * 0.034 / 2;
  return distancia;
}

// Actualizar lógica automática
void Ascensor::actualizar() {
  // Actualizar lectura de distancia
  distanciaActual = medirDistancia();
  
  // Si no hay destino o no estamos en modo automático, salir
  if (destino < 0 || !powerOn || modoManual) return;
  
  float diferencia = destino - distanciaActual;
  
  // Verificar si llegó al destino
  if (abs(diferencia) <= 0.5) {
    digitalWrite(pinSubirLento, HIGH);
    digitalWrite(pinBajarLento, HIGH);
    digitalWrite(pinRapido, HIGH);
    estadoMotor = "Parado";
    velocidad = "-";
    direccion = "-";
    destino = -1;
    return;
  }
  
  // Establecer dirección
  if (diferencia > 0) {
    direccion = "Subida";
    digitalWrite(pinBajarLento, HIGH);
    digitalWrite(pinSubirLento, LOW);
  } else {
    direccion = "Bajada";
    digitalWrite(pinSubirLento, HIGH);
    digitalWrite(pinBajarLento, LOW);
  }
  
  estadoMotor = "Arrancado";
  
  // Establecer velocidad según distancia restante
  float distanciaRestante = abs(diferencia);
  if (distanciaRestante > 6.0) {
    digitalWrite(pinRapido, LOW);
    velocidad = "Rápido";
  } else {
    digitalWrite(pinRapido, HIGH);
    velocidad = "Lento";
  }
}

// Getters
float Ascensor::getDistanciaActual() {
  return distanciaActual;
}

float Ascensor::getDestino() {
  return destino;
}

String Ascensor::getEstadoMotor() {
  return estadoMotor;
}

String Ascensor::getVelocidad() {
  return velocidad;
}

String Ascensor::getDireccion() {
  return direccion;
}
