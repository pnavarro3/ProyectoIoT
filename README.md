# Sistema IoT de Control de Montacargas

## Resumen

Este proyecto implementa un sistema de control automatizado para un montacargas industrial equipado con motor Dahlander. La arquitectura se basa en dos microcontroladores ESP32 que se comunican mediante el protocolo MQTT, permitiendo el monitoreo en tiempo real y el control remoto a través de una interfaz web.

## Componentes del Sistema

### ESP32 #1: Módulo de Sensores
**Función:** Adquisición y transmisión de datos ambientales y de posición

**Hardware integrado:**
- Sensor ultrasónico HC-SR04: Medición de distancia para determinación de posición
- Fotoresistor LDR con comparador: Detección de condiciones de iluminación ambiental
- Sensor DHT11: Monitoreo de temperatura y humedad relativa

**Operación:**
- Frecuencia de muestreo: 250ms
- Filtro digital de mediana (n=5) aplicado a mediciones ultrasónicas
- Transmisión de datos mediante protocolo MQTT al broker central

### ESP32 #2: Módulo de Control
**Función:** Control del sistema motriz y provisión de interfaces de usuario

**Hardware integrado:**
- Módulo de relés HW-316 (4 canales): Control del motor Dahlander mediante conmutación de bobinados para velocidad variable
- Display LCD I2C 16x2: Visualización local del estado operativo
- LED indicador: Iluminación automática regulada por sensor de luminosidad

**Operación:**
- Recepción de telemetría vía MQTT
- Algoritmo de control de posición basado en retroalimentación del sensor ultrasónico
- Servidor HTTP embebido para interfaz de usuario remota
- Actualización continua de display LCD

## Principio de Funcionamiento

### Sistema de Referencia de Posición

El sistema utiliza mediciones de distancia para determinar la planta actual del montacargas:

| Planta | Distancia | Tolerancia |
|:------:|:---------:|:----------:|
| 0      | 5 cm      | ±1 cm      |
| 1      | 20 cm     | ±1 cm      |
| 2      | 35 cm     | ±1 cm      |
| 3      | 50 cm     | ±1 cm      |

### Algoritmo de Control de Velocidad

El sistema implementa un perfil de velocidad trapezoidal para optimizar el tiempo de desplazamiento minimizando las oscilaciones:

- **Fase de aceleración** (primeros 4 cm): Velocidad BAJA para arranque suave
- **Fase de crucero**: Velocidad ALTA para desplazamiento eficiente
- **Fase de desaceleración** (últimos 4 cm): Velocidad BAJA para posicionamiento preciso

Precisión de parada: ±1 cm respecto al objetivo

### Mecanismos de Seguridad

- Bloqueo de comandos durante operación en movimiento
- Sistema anti-oscilación mediante flag de estado
- Validación de integridad de datos del sensor
- Reconexión automática al broker MQTT en caso de pérdida de conexión

### Control del Motor Dahlander

El sistema utiliza un motor trifásico Dahlander, caracterizado por su capacidad de operar a dos velocidades diferentes mediante la conmutación de sus bobinados. El control se realiza a través de 4 relés que gestionan:

- **Dirección de movimiento:** Subida o bajada del montacargas mediante inversión de fases
- **Velocidad de operación:** Conmutación entre velocidad lenta y rápida según la configuración de polos del motor
- **Alimentación general:** Control de potencia al sistema motriz

La lógica de control implementa un perfil de velocidad variable que aprovecha las dos velocidades del motor Dahlander:
- Velocidad BAJA (más polos): Utilizada en arranque y frenado para posicionamiento preciso
- Velocidad ALTA (menos polos): Empleada durante el desplazamiento medio para mayor eficiencia

La conmutación entre velocidades se realiza mediante secuencias controladas de activación/desactivación de relés, siguiendo las especificaciones del fabricante del motor para evitar transitorios eléctricos y mecánicos perjudiciales.

## Interfaz de Usuario Web

La interfaz web proporciona acceso remoto al sistema desde cualquier dispositivo con navegador en la red local.

**Funcionalidades implementadas:**

1. **Panel de telemetría en tiempo real:**
   - Indicador de planta actual
   - Lecturas de temperatura y humedad ambiental
   - Estado del sensor de luminosidad (Claro/Oscuro)
   - Estado operativo del sistema (Parado/Subiendo/Bajando)

2. **Panel de control:**
   - Cuatro botones de selección de planta destino (0, 1, 2, 3)
   - Control maestro de encendido/apagado del sistema
   - Indicadores de estado bloqueado durante operación

3. **Sistema de retroalimentación visual:**
   - Aviso dinámico durante desplazamiento
   - Deshabilitación de controles durante operación activa
   - Actualización asíncrona vía AJAX (frecuencia: 1 Hz)

## Arquitectura de Comunicación MQTT

```
┌─────────────────────────┐
│  ESP32 Sensores         │
│  - Adquisición de datos │
│  - Publicador MQTT      │
└────────────┬────────────┘
             │ Publica (250ms)
             ↓
┌────────────────────────────┐
│ Broker MQTT (Mosquitto)    │
│ Puerto: 1883               │
│ Host: Computadora portátil │
└────────────┬───────────────┘
             │ Retransmite
             ↓
┌─────────────────────────┐
│ ESP32 Montacargas       │
│ - Suscriptor MQTT       │
│ - Control motriz        │
│ - Servidor HTTP         │
│ - Interfaz LCD          │
└─────────────────────────┘
```

### Tópicos MQTT Implementados

| Tópico                                  | Función                   | Formato de Datos            |
|:----------------------------------------|:--------------------------|:---------------------------:|
| `montacargas/sensores/distancia`        | Posición del montacargas  | Float (cm)                  |
| `montacargas/sensores/luminosidad`      | Estado de iluminación     | Int (0=Claro, 1=Oscuro)     |
| `montacargas/sensores/temperatura`      | Temperatura ambiental     | Float (°C)                  |
| `montacargas/sensores/humedad`          | Humedad relativa          | Float (%)                   |

## Procedimiento de Configuración

### 1. Instalación del Broker MQTT

El sistema utiliza Mosquitto como broker MQTT. Procedimiento de instalación en Windows:

1. Descargar Mosquitto desde: https://mosquitto.org/download/
2. Crear archivo de configuración en `C:\Program Files\mosquitto\mosquitto.conf`:
```
listener 1883
allow_anonymous true
```
3. Configurar firewall de Windows:
```powershell
New-NetFirewallRule -DisplayName "MQTT" -Direction Inbound -Protocol TCP -LocalPort 1883 -Action Allow
```
4. Iniciar servicio:
```powershell
net start mosquitto
```

### 2. Configuración de Red

Obtener dirección IPv4 del computador que ejecuta el broker:
```powershell
ipconfig
```
Anotar la dirección IPv4 (ejemplo: 192.168.1.100)

### 3. Configuración del Firmware

Modificar los siguientes parámetros en ambos archivos .ino:

**SensoresESP.ino:**
```cpp
const char* ssid = "NOMBRE_RED_WIFI";
const char* password = "CONTRASEÑA_WIFI";
const char* mqtt_server = "IP_DEL_BROKER"; // Ejemplo: 192.168.1.100
```

**Montacargas.ino:**
```cpp
const char* ssid = "NOMBRE_RED_WIFI";
const char* password = "CONTRASEÑA_WIFI";
const char* mqtt_server = "IP_DEL_BROKER"; // Ejemplo: 192.168.1.100
```

### 4. Dependencias de Software

Instalar las siguientes bibliotecas mediante el Gestor de Bibliotecas del IDE de Arduino:

**Para SensoresESP:**
- WiFi (incluida en el núcleo ESP32)
- PubSubClient by Nick O'Leary
- DHT sensor library by Adafruit
- Adafruit Unified Sensor

**Para Montacargas:**
- WiFi (incluida en el núcleo ESP32)
- WebServer (incluida en el núcleo ESP32)
- PubSubClient by Nick O'Leary
- LiquidCrystal_I2C by Frank de Brabander

### 5. Carga del Firmware y Conexión de Hardware

Compilar y cargar el código correspondiente a cada ESP32 mediante el IDE de Arduino.

## Esquema de Conexiones

### Módulo de Sensores (ESP32 #1)

| Componente       | Pin ESP32  | Función                                  |
|:-----------------|:----------:|:-----------------------------------------|
| HC-SR04 TRIG     | GPIO 5     | Trigger del sensor ultrasónico           |
| HC-SR04 ECHO     | GPIO 18    | Echo del sensor ultrasónico              |
| LDR              | GPIO 34    | Entrada digital (módulo con comparador)  |
| DHT11            | GPIO 4     | Bus de datos del sensor                  |

### Módulo de Control (ESP32 #2)

| Componente       | Pin ESP32  | Función                                  |
|:-----------------|:----------:|:-----------------------------------------|
| Relé 1           | GPIO 26    | Control motor Dahlander - Subida lenta   |
| Relé 2           | GPIO 25    | Control motor Dahlander - Bajada lenta   |
| Relé 3           | GPIO 27    | Control motor Dahlander - Velocidad alta |
| Relé 4           | GPIO 14    | Control motor Dahlander - Alimentación   |
| LCD SDA          | GPIO 21    | Bus I2C datos                            |
| LCD SCL          | GPIO 22    | Bus I2C reloj                            |
| LED              | GPIO 2     | Indicador de iluminación                 |

**Nota:** El LCD I2C debe estar configurado en la dirección 0x27. Si no funciona, intentar con 0x3F.

## Procedimiento de Operación

1. Iniciar el servicio del broker MQTT en el computador host
2. Alimentar ambos microcontroladores ESP32
3. Verificar establecimiento de conexión WiFi y MQTT (observable en el LCD del módulo de control)
4. Acceder a la interfaz web mediante navegador: `http://[IP_DEL_ESP32_MONTACARGAS]`
5. Activar el sistema mediante el control maestro en la interfaz
6. Seleccionar la planta destino mediante los botones correspondientes
7. El sistema ejecutará el desplazamiento automáticamente aplicando el perfil de velocidad programado

## Estados del Display LCD

El display LCD de 16x2 caracteres muestra información contextual según el estado del sistema:

| Estado                 | Línea 1            | Línea 2            |
|:-----------------------|:-------------------|:-------------------|
| Sistema desactivado    | "Sistema Apagado"  | -                  |
| Ascenso en progreso    | "  SUBIENDO..."    | "Espere por favor" |
| Descenso en progreso   | "  BAJANDO..."     | "Espere por favor" |
| En posición            | "Planta: [N]"      | "** DISPONIBLE **" |

## Diagnóstico y Resolución de Problemas

### Problema: LED de iluminación no responde
**Diagnóstico:**
- Verificar integridad del módulo LDR
- Comprobar conexión en GPIO34
- Validar funcionamiento del comparador de umbral

### Problema: Sistema de control no responde a comandos
**Diagnóstico:**
- Verificar estado de conexión MQTT mediante monitor serial
- Confirmar que el servicio del broker está activo
- Validar configuración de red (dirección IP correcta)

### Problema: Display LCD sin visualización
**Diagnóstico:**
- Modificar dirección I2C de 0x27 a 0x3F en el código
- Verificar integridad de conexiones SDA (GPIO21) y SCL (GPIO22)
- Comprobar alimentación del módulo LCD

### Problema: Mediciones inconsistentes del sensor ultrasónico
**Diagnóstico:**
- El filtro de mediana implementado debería mitigar valores anómalos
- Verificar alimentación estable del sensor (VCC: 5V)
- Comprobar ausencia de obstáculos en el cono de medición

## Especificaciones Técnicas

| Parámetro                                    | Valor | Unidad    |
|:---------------------------------------------|:-----:|:---------:|
| Frecuencia de muestreo                       | 250   | ms        |
| Tasa de mensajes MQTT                        | 4     | msg/s     |
| Tamaño de ventana del filtro de mediana      | 5     | muestras  |
| Tolerancia de posicionamiento                | ±1    | cm        |
| Longitud de zona de velocidad reducida       | 4     | cm        |
| Frecuencia de actualización de interfaz web  | 1     | Hz        |
| Puerto del broker MQTT                       | 1883  | -         |
| Dirección I2C del LCD                        | 0x27  | hex       |
| Precisión del sensor HC-SR04                 | 2-400 | cm        |

### Características del Sistema de Reconexión

El sistema implementa reconexión automática al broker MQTT en caso de pérdida de conectividad, con reintentos cada 5 segundos hasta restablecer la comunicación.

## Conclusión

Este proyecto demuestra la implementación práctica de un sistema IoT para automatización industrial, integrando múltiples sensores, control en tiempo real y comunicación mediante protocolo MQTT. El diseño modular permite escalabilidad y mantenimiento eficiente del sistema.

## Referencias

- Documentación oficial ESP32: https://docs.espressif.com/
- Protocolo MQTT v3.1.1: http://docs.oasis-open.org/mqtt/
- Biblioteca PubSubClient: https://pubsubclient.knolleary.net/
