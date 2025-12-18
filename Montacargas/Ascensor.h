#ifndef ASCENSOR_H
#define ASCENSOR_H

#include <Arduino.h>

class Ascensor {
private:
  // Pines
  int pinSubirLento;
  int pinBajarLento;
  int pinRapido;
  int pinPower;
  int pinTrig;
  int pinEcho;
  
  // Estado
  float distanciaActual;
  float destino;
  String estadoMotor;
  String velocidad;
  String direccion;
  bool powerOn;
  bool modoManual;
  
  // Métodos privados auxiliares
  void activarReleDireccion(bool subir);
  void desactivarReles();
  
public:
  // Constructor
  Ascensor(int rele1, int rele2, int rele3, int rele4, int trig, int echo);
  
  // Inicialización
  void inicializar();
  
  // Control de power
  void encender();
  void apagar();
  bool estaEncendido();
  
  // Modo de operación
  void setModoManual();
  void setModoAutomatico();
  bool esModoManual();
  
  // Control manual
  void subirLento();
  void bajarLento();
  void activarRapido();
  void parar();
  
  // Control automático
  void irAPlanta(float distanciaDestino);
  void actualizar(); // Ejecutar lógica automática
  
  // Sensor
  float medirDistancia();
  
  // Getters
  float getDistanciaActual();
  float getDestino();
  String getEstadoMotor();
  String getVelocidad();
  String getDireccion();
};

#endif
