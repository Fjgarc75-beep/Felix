/*
 * =====================================================================
 *  ETAPA 1 - Verificación básica del ESP32 y comunicación serie
 * =====================================================================
 *  Proyecto : Monitor experimental de frecuencia cardiaca y SpO2
 *  Materia  : Sistemas de Información Hospitalaria
 *             Maestría en Ingeniería Clínica
 *  Placa    : NodeMCU ESP-32S (ESP32-WROOM-32)
 *  Entorno  : Arduino IDE + paquete "esp32 by Espressif Systems"
 *
 *  PROTOTIPO ACADÉMICO Y EXPERIMENTAL.
 *  NO es un equipo médico certificado ni un dispositivo de diagnóstico.
 *
 *  Qué hace este programa:
 *   1. Imprime información del chip en el Monitor Serie (115200 baudios).
 *   2. Hace parpadear el LED integrado (GPIO 2) SIN usar delay(),
 *      con la técnica no bloqueante basada en millis().
 *   3. Envía un mensaje de estado ("latido") cada segundo.
 *   4. Recibe comandos desde el Monitor Serie (comunicación bidireccional):
 *        h -> ayuda
 *        i -> volver a mostrar la información del chip
 *        l -> activar / desactivar el parpadeo del LED
 *
 *  Conexiones: SOLO el cable USB. No conectar el MAX30102 ni la TFT.
 * =====================================================================
 */

#include <Arduino.h>
#include <esp_system.h>   // esp_reset_reason()

// ------------------------- Configuración -------------------------
const uint8_t  LED_PIN            = 2;       // LED integrado de la NodeMCU ESP-32S
const uint32_t SERIAL_BAUD        = 115200;  // Debe coincidir con el Monitor Serie
const uint32_t LED_INTERVAL_MS    = 500;     // Cambia de estado cada 500 ms -> 1 parpadeo/s
const uint32_t STATUS_INTERVAL_MS = 1000;    // Mensaje de estado cada 1 s

// ------------------------- Variables de estado -------------------
uint32_t lastLedToggleMs = 0;
uint32_t lastStatusMs    = 0;
uint32_t heartbeatCount  = 0;
bool     ledOn           = false;
bool     blinkEnabled    = true;

// -----------------------------------------------------------------
// Traduce el motivo del último reinicio a texto.
// Será útil en etapas posteriores: por ejemplo, un reinicio por
// "caída de voltaje" (brownout) indicaría problemas de alimentación.
// -----------------------------------------------------------------
const char* resetReasonText(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON:   return "Encendido (power-on)";
    case ESP_RST_EXT:       return "Reset externo";
    case ESP_RST_SW:        return "Reinicio por software";
    case ESP_RST_PANIC:     return "Error del programa (panic)";
    case ESP_RST_INT_WDT:   return "Watchdog de interrupciones";
    case ESP_RST_TASK_WDT:  return "Watchdog de tareas";
    case ESP_RST_WDT:       return "Otro watchdog";
    case ESP_RST_DEEPSLEEP: return "Salida de deep sleep";
    case ESP_RST_BROWNOUT:  return "Caida de voltaje (brownout)";
    case ESP_RST_SDIO:      return "Reset por SDIO";
    default:                return "Desconocido";
  }
}

// -----------------------------------------------------------------
// Muestra la información del chip.
// -----------------------------------------------------------------
void printChipInfo() {
  Serial.println();
  Serial.println("================================================");
  Serial.println(" ETAPA 1 - Verificacion basica del ESP32");
  Serial.println(" Prototipo academico - NO es dispositivo medico");
  Serial.println("================================================");

  Serial.printf("Modelo del chip       : %s\n", ESP.getChipModel());
  Serial.printf("Revision del chip     : %d\n", (int)ESP.getChipRevision());
  Serial.printf("Nucleos               : %d\n", (int)ESP.getChipCores());
  Serial.printf("Frecuencia CPU        : %lu MHz\n", (unsigned long)ESP.getCpuFreqMHz());
  Serial.printf("Memoria flash         : %lu MB\n",
                (unsigned long)(ESP.getFlashChipSize() / (1024UL * 1024UL)));
  Serial.printf("RAM (heap) total      : %lu bytes\n", (unsigned long)ESP.getHeapSize());
  Serial.printf("RAM (heap) libre      : %lu bytes\n", (unsigned long)ESP.getFreeHeap());

  // PSRAM: los módulos WROVER la tienen y usan internamente GPIO16/17.
  // En un WROOM-32 debe ser 0, lo que confirma que GPIO16/17 están libres
  // para las líneas A0 (DC) y RESET de la TFT.
  uint32_t psram = ESP.getPsramSize();
  Serial.printf("PSRAM                 : %lu bytes", (unsigned long)psram);
  if (psram == 0) {
    Serial.println("  -> OK: GPIO16/17 libres para la TFT");
  } else {
    Serial.println("  -> ATENCION: modulo con PSRAM, GPIO16/17 podrian estar ocupados");
  }

  // Dirección MAC grabada de fábrica (identificador único de la placa)
  uint64_t mac = ESP.getEfuseMac();
  Serial.printf("MAC (eFuse)           : %02X:%02X:%02X:%02X:%02X:%02X\n",
                (unsigned)((mac >> 0)  & 0xFF), (unsigned)((mac >> 8)  & 0xFF),
                (unsigned)((mac >> 16) & 0xFF), (unsigned)((mac >> 24) & 0xFF),
                (unsigned)((mac >> 32) & 0xFF), (unsigned)((mac >> 40) & 0xFF));

  Serial.printf("Version ESP-IDF       : %s\n", ESP.getSdkVersion());
#ifdef ESP_ARDUINO_VERSION_MAJOR
  Serial.printf("Nucleo Arduino-ESP32  : %d.%d.%d\n",
                ESP_ARDUINO_VERSION_MAJOR, ESP_ARDUINO_VERSION_MINOR, ESP_ARDUINO_VERSION_PATCH);
#endif
  Serial.printf("Motivo del reinicio   : %s\n", resetReasonText(esp_reset_reason()));
  Serial.println("------------------------------------------------");
  Serial.println("Escriba 'h' y presione Enviar para ver los comandos.");
  Serial.println();
}

void printHelp() {
  Serial.println();
  Serial.println("Comandos disponibles:");
  Serial.println("  h -> mostrar esta ayuda");
  Serial.println("  i -> mostrar informacion del chip");
  Serial.println("  l -> activar / desactivar el parpadeo del LED");
  Serial.println();
}

// -----------------------------------------------------------------
// Lee los caracteres recibidos por el puerto serie y ejecuta comandos.
// Se ignoran los fines de línea (\r, \n) que agrega el Monitor Serie.
// -----------------------------------------------------------------
void handleSerialCommands() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r' || c == '\n' || c == ' ') continue;

    switch (c) {
      case 'h': case 'H':
        printHelp();
        break;
      case 'i': case 'I':
        printChipInfo();
        break;
      case 'l': case 'L':
        blinkEnabled = !blinkEnabled;
        if (!blinkEnabled) {            // al desactivar, deja el LED apagado
          ledOn = false;
          digitalWrite(LED_PIN, LOW);
        }
        Serial.printf(">> Parpadeo del LED %s\n", blinkEnabled ? "ACTIVADO" : "DESACTIVADO");
        break;
      default:
        Serial.printf(">> Comando no reconocido: '%c' (escriba 'h' para ayuda)\n", c);
        break;
    }
  }
}

// -----------------------------------------------------------------
// Parpadeo no bloqueante: solo cambia el LED cuando ha transcurrido
// el intervalo. El programa nunca se detiene esperando.
// -----------------------------------------------------------------
void updateLed(uint32_t nowMs) {
  if (!blinkEnabled) return;
  if (nowMs - lastLedToggleMs >= LED_INTERVAL_MS) {
    lastLedToggleMs = nowMs;
    ledOn = !ledOn;
    digitalWrite(LED_PIN, ledOn ? HIGH : LOW);
  }
}

// -----------------------------------------------------------------
// Mensaje periódico de estado: confirma que el lazo principal sigue
// ejecutándose y permite vigilar la memoria libre.
// -----------------------------------------------------------------
void updateStatus(uint32_t nowMs) {
  if (nowMs - lastStatusMs >= STATUS_INTERVAL_MS) {
    lastStatusMs = nowMs;
    heartbeatCount++;
    Serial.printf("[t=%lu s] latido #%lu | heap libre: %lu bytes | LED %s\n",
                  (unsigned long)(nowMs / 1000UL),
                  (unsigned long)heartbeatCount,
                  (unsigned long)ESP.getFreeHeap(),
                  blinkEnabled ? "parpadeando" : "apagado");
  }
}

// =================================================================
void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.begin(SERIAL_BAUD);
  delay(1000);   // Solo en setup: da tiempo a abrir el Monitor Serie

  printChipInfo();

  uint32_t now = millis();
  lastLedToggleMs = now;
  lastStatusMs    = now;
}

void loop() {
  uint32_t now = millis();   // Tiempo desde el arranque, en milisegundos

  handleSerialCommands();
  updateLed(now);
  updateStatus(now);
}
