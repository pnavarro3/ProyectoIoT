// Pines asignados
const int RELE_ESTRELLA = 5;
const int RELE_TRIANGULO = 18;
const int RELE_GIRO_ADELANTE = 19;
const int RELE_GIRO_ATRAS = 21;

void setup() {
  Serial.begin(115200);

  // Configurar pines como salida
  pinMode(RELE_ESTRELLA, OUTPUT);
  pinMode(RELE_TRIANGULO, OUTPUT);
  pinMode(RELE_GIRO_ADELANTE, OUTPUT);
  pinMode(RELE_GIRO_ATRAS, OUTPUT);

  // Apagar todo al inicio
  apagarTodo();
}

void loop() {
  // Ejemplo: arranque en estrella, luego triángulo, giro adelante
  arrancarMotor(true); // true = adelante, false = atrás

  delay(10000); // Mantener motor encendido por 10 segundos

  apagarTodo();
  delay(10000); // Esperar antes de repetir
}

void arrancarMotor(bool adelante) {
  // Seleccionar sentido de giro
  if (adelante) {
    digitalWrite(RELE_GIRO_ADELANTE, HIGH);
    digitalWrite(RELE_GIRO_ATRAS, LOW);
  } else {
    digitalWrite(RELE_GIRO_ADELANTE, LOW);
    digitalWrite(RELE_GIRO_ATRAS, HIGH);
  }

  // Arranque en estrella
  digitalWrite(RELE_ESTRELLA, HIGH);
  digitalWrite(RELE_TRIANGULO, LOW);
  delay(3000); // Esperar 3 segundos

  // Conmutar a triángulo
  digitalWrite(RELE_ESTRELLA, LOW);
  digitalWrite(RELE_TRIANGULO, HIGH);
}

void apagarTodo() {
  digitalWrite(RELE_ESTRELLA, LOW);
  digitalWrite(RELE_TRIANGULO, LOW);
  digitalWrite(RELE_GIRO_ADELANTE, LOW);
  digitalWrite(RELE_GIRO_ATRAS, LOW);
}
