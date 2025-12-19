# Sistema IoT de Control de Montacargas

## Resumen

Este proyecto implementa un sistema de control automatizado para un montacargas industrial equipado con motor Dahlander. La arquitectura se basa en dos microcontroladores ESP32 que se comunican mediante el protocolo MQTT, permitiendo el monitoreo en tiempo real y el control remoto a través de una interfaz web. El sistema incluye integración con News API para mostrar noticias actualizadas sobre IoT en la interfaz de usuario.

## Componentes del Sistema

### ESP32 #1: Módulo de Sensores
- **HC-SR04:** Medición de distancia (filtro de mediana con 5 muestras)
- **LDR:** Detección de luminosidad ambiente
- **DHT11:** Temperatura y humedad
- **Función:** Publicación de datos vía MQTT cada 2 segundos

### ESP32 #2: Módulo de Control  
- **Relés HW-316 (4 canales):** Control de motor Dahlander (2 velocidades)
- **LCD I2C 16x2:** Visualización de estado
- **LED:** Iluminación automática según luminosidad
- **Función:** Servidor web, control de relés, suscripción MQTT

## Principio de Funcionamiento

**Plantas definidas por distancia:** P0=5cm, P1=20cm, P2=35cm, P3=50cm (tolerancia ±2cm)

**Control de velocidad trapezoidal:**
- Primeros 4cm: Velocidad BAJA (arranque suave)
- Trayecto medio: Velocidad ALTA (eficiencia)
- Últimos 4cm: Velocidad BAJA (precisión)

**Motor Dahlander:** 4 relés controlan dirección (subir/bajar), velocidad (lenta/rápida) y alimentación general.

**Seguridad:** Bloqueo durante movimiento, validación de sensor, reconexión automática MQTT.

## Interfaz de Usuario Web

Acceso remoto al sistema en `http://[IP_ESP32]`

**Características:**
- Telemetría en tiempo real (planta, temperatura, humedad, luz)
- Botones de control para seleccionar planta destino (0-3)
- Encendido/apagado del sistema
- Widget de noticias IoT (últimas 5 noticias, actualización cada 5 min)
- Actualización asíncrona AJAX (1 Hz)

**Endpoints principales:** `/` (interfaz), `/togglepower`, `/planta[0-3]`, `/estado` (JSON), `/noticias`, `/actualizarnoticias`

## Integración con News API

Widget de noticias IoT en la interfaz web (5 noticias en español, actualización cada 5 min).

**Configuración:** Obtener API Key gratuita en https://newsapi.org y configurar en `Montacargas.ino`:
```cpp
const char* newsApiKey = "TU_API_KEY";
```

## Estructura del Proyecto

```
ProyectoIoT/
├── README.md                          # Documentación completa del proyecto
├── INSTRUCCIONES_BROKER_MQTT.txt     # Guía de configuración del broker MQTT
├── Informe_IoT.tex                   # Informe técnico en LaTeX
├── CADe_SIMU_Codigo4962/             # Simulación CADe SIMU
│   └── ProyectoIoT.cad
├── Montacargas/                      # ESP32 Control Principal
│   ├── Montacargas.ino              # Código principal del montacargas
│   ├── Ascensor.h                   # Clase auxiliar (encabezado)
│   └── Ascensor.cpp                 # Clase auxiliar (implementación)
└── SensoresESP/                      # ESP32 Módulo de Sensores
    └── SensoresESP.ino              # Código de adquisición de datos
```

## Arquitectura MQTT

**Flujo:** ESP32 Sensores → Broker MQTT (Mosquitto:1883) → ESP32 Montacargas

**Tópicos:** `montacargas/sensores/{distancia, luminosidad, temperatura, humedad}`

## Configuración Rápida

### 1. Broker MQTT
- Instalar Mosquitto (https://mosquitto.org)
- Configurar puerto 1883 y abrir firewall
- Obtener IP del broker con `ipconfig`

### 2. Firmware
Actualizar en ambos `.ino`:
```cpp
const char* mqtt_server = "IP_BROKER";
const char* newsApiKey = "TU_API_KEY"; // Solo Montacargas
```

### 3. Librerías Arduino
**Ambos ESP32:** PubSubClient, DHT sensor library, Adafruit Unified Sensor  
**Montacargas:** LiquidCrystal_I2C, ArduinoJson

## Pines Principales

**Sensores:** HC-SR04 (GPIO 5/18), LDR (GPIO 34), DHT11 (GPIO 4)  
**Control:** Relés (GPIO 26/25/27/14), LCD I2C (GPIO 21/22, dir. 0x27), LED (GPIO 2)

## Troubleshooting

- **LCD sin imagen:** Cambiar dirección I2C a 0x3F
- **Sin conexión MQTT:** Verificar IP del broker y firewall
- **Noticias no cargan:** Validar API Key y límite diario (100 req/día)
- **Sensor inconsistente:** Verificar alimentación 5V y ausencia de obstáculos

## Seguridad

⚠️ **Producción:** Usar archivo `config.h` separado con credenciales, excluirlo de git, e implementar autenticación MQTT.

## Arquitectura del Código

**Montacargas.ino:** Implementación principal (servidor web, MQTT, control de relés, News API, LCD)  
**Ascensor.h/cpp:** Clases C++ modulares (no utilizadas actualmente, disponibles para refactorización)

## Especificaciones

- **Sensores:** Muestreo cada 2s, filtro mediana (5 muestras)
- **Posicionamiento:** Tolerancia ±2cm, zona lenta 4cm
- **MQTT:** Puerto 1883, reconexión automática
- **Web:** Actualización 1Hz, noticias cada 5min
- **LCD:** I2C 0x27, 16x2 caracteres

## Mejoras Futuras

- Autenticación MQTT/HTTPS, logs en SD, alarmas configurables
- Dashboard responsive, gráficos históricos, modo oscuro
- Sensores adicionales (peso, acelerómetro), cámara, batería backup

## Referencias

- ESP32: https://docs.espressif.com
- MQTT: http://docs.oasis-open.org/mqtt
- News API: https://newsapi.org/docs
- PubSubClient: https://pubsubclient.knolleary.net
