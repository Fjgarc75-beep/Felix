/*
 * =====================================================================
 *  ETAPA 2.1 - Verificación simplificada de la comunicación I2C
 *              con el MAX30102
 * =====================================================================
 *  Proyecto : Monitor experimental de frecuencia cardiaca y SpO2
 *  Materia  : Sistemas de Información Hospitalaria
 *             Maestría en Ingeniería Clínica
 *  Placa    : NodeMCU ESP-32S (ESP32-WROOM-32)
 *  Entorno  : Arduino IDE, Arduino-ESP32 3.3.x (ESP-IDF 5.5)
 *  Librería : SparkFun MAX3010x Pulse and Proximity Sensor Library
 *             (MAX30105.h, compatible con MAX30102) + Wire
 *
 *  PROTOTIPO ACADÉMICO Y EXPERIMENTAL.
 *  NO es un equipo médico certificado ni un dispositivo de diagnóstico.
 *
 *  Conexiones (SIN TFT):
 *    MAX30102 VIN -> 3V3      MAX30102 SDA -> GPIO 21
 *    MAX30102 GND -> GND      MAX30102 SCL -> GPIO 22
 *    INT y RD sin conectar
 *
 *  Pruebas:
 *    1. Inicio del bus I2C a 100 kHz
 *    2. Detección del dispositivo en 0x57
 *    3. PART_ID = 0x15
 *    4. REV_ID (solo informativo)
 *    5. 200 lecturas consecutivas de PART_ID a 100 kHz
 *    6. 200 lecturas consecutivas de PART_ID a 400 kHz
 *    7. Inicialización con particleSensor.begin()
 *    8. Confirmación final con particleSensor.readPartID()
 *
 *  Este programa SOLO LEE registros: no escribe ningún registro del
 *  sensor, no enciende los LED y no mide temperatura.
 *  NO se calcula BPM ni SpO2 ni se adquiere la señal PPG.
 *
 *  Monitor Serie a 115200 baudios. Comandos:
 *    t -> repetir las pruebas
 *    h -> ayuda
 * =====================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include "MAX30105.h"

// ------------------------- Configuración -------------------------
const int      PIN_SDA           = 21;
const int      PIN_SCL           = 22;
const uint32_t I2C_100K          = 100000;
const uint32_t I2C_400K          = 400000;
const uint16_t I2C_TIMEOUT_MS    = 50;
const uint32_t SERIAL_BAUD       = 115200;
const int      N_READS           = 200;
const int      DETECT_ATTEMPTS   = 3;
const uint32_t MONITOR_PERIOD_MS = 5000;

const uint8_t  MAX30102_ADDR     = 0x57;
const uint8_t  EXPECTED_PART_ID  = 0x15;
const uint8_t  REG_REV_ID        = 0xFE;
const uint8_t  REG_PART_ID       = 0xFF;

MAX30105 particleSensor;

// ------------------------- Estado --------------------------------
bool     busStarted     = false;   // Wire.begin() se llama UNA sola vez
uint8_t  lastI2cError   = 0;       // último código de error I2C
uint32_t lastMonitorMs  = 0;
uint32_t monitorCount   = 0;
uint32_t monitorErrors  = 0;

// =================================================================
//  Acceso a registros (solo lectura)
// =================================================================
// Secuencia I2C: START | 0x57+W | registro | RESTART | 0x57+R | dato | STOP
bool readRegister(uint8_t reg, uint8_t &value) {
  Wire.beginTransmission(MAX30102_ADDR);
  Wire.write(reg);
  uint8_t err = Wire.endTransmission(false);    // false = repeated start
  if (err != 0) { lastI2cError = err; return false; }
  if (Wire.requestFrom(MAX30102_ADDR, (size_t)1) != 1) { lastI2cError = 2; return false; }
  if (Wire.available() < 1) { lastI2cError = 4; return false; }
  value = (uint8_t)Wire.read();
  return true;
}

const char* i2cErrorText(uint8_t code) {
  switch (code) {
    case 0:  return "sin error";
    case 2:  return "NACK: el dispositivo no respondio";
    case 3:  return "NACK en datos";
    case 5:  return "tiempo agotado (bus bloqueado)";
    default: return "error del bus";
  }
}

void printResult(int n, const char* name, bool ok) {
  Serial.printf("  [%s] %d. %s\n", ok ? " OK " : "FALLA", n, name);
}

// =================================================================
//  Lecturas consecutivas de PART_ID a una velocidad dada
// =================================================================
// Nota: en un .ino, Arduino IDE genera automáticamente los prototipos
// de las funciones y los coloca al inicio del archivo. Por eso esta
// función usa solo tipos básicos (bool, int, float, uint32_t) y no
// estructuras propias en sus parámetros ni en su valor de retorno.
//
// Cambia la velocidad del bus, hace N_READS lecturas de PART_ID,
// muestra las estadísticas y devuelve true si no hubo ningún error.
bool readPartIdRepeated(uint32_t freq) {
  bool clockOk    = Wire.setClock(freq);
  int commErrors  = 0;   // lecturas sin respuesta
  int wrongValues = 0;   // respuestas con valor distinto de 0x15

  uint32_t t0 = micros();
  for (int i = 0; i < N_READS; i++) {
    uint8_t v = 0;
    if (!readRegister(REG_PART_ID, v)) {
      commErrors++;
    } else if (v != EXPECTED_PART_ID) {
      wrongValues++;
    }
  }
  float usPerRead = (float)(micros() - t0) / N_READS;

  Serial.printf("      Velocidad solicitada %lu Hz, configurada %lu Hz (setClock %s)\n",
                (unsigned long)freq, (unsigned long)Wire.getClock(), clockOk ? "OK" : "FALLO");
  Serial.printf("      Correctas: %d/%d | sin respuesta: %d | valor incorrecto: %d | %.0f us/lectura\n",
                N_READS - commErrors - wrongValues, N_READS,
                commErrors, wrongValues, usPerRead);
  if (commErrors > 0) {
    Serial.printf("      Ultimo error I2C: codigo %u (%s)\n", lastI2cError, i2cErrorText(lastI2cError));
  }

  return clockOk && (commErrors == 0) && (wrongValues == 0);
}

// =================================================================
//  Rutina de pruebas
// =================================================================
void runTests() {
  bool ok[9] = {false};     // ok[1]..ok[8]
  uint8_t partId = 0, revId = 0;
  bool revRead = false;
  lastI2cError = 0;

  Serial.println();
  Serial.println("================================================");
  Serial.println(" ETAPA 2.1 - Comunicacion I2C con el MAX30102");
  Serial.println(" Prototipo academico - NO es dispositivo medico");
  Serial.println("================================================");

  // ---- 1. Bus I2C a 100 kHz ----
  if (!busStarted) {
    busStarted = Wire.begin(PIN_SDA, PIN_SCL, I2C_100K);
    Wire.setTimeOut(I2C_TIMEOUT_MS);
    delay(10);   // margen para el sensor tras el arranque
  }
  bool clk100 = Wire.setClock(I2C_100K);
  ok[1] = busStarted && clk100;
  Serial.printf("\n  1. Wire.begin(SDA=%d, SCL=%d) a 100 kHz: %s | reloj: %lu Hz\n",
                PIN_SDA, PIN_SCL, busStarted ? "OK" : "FALLO", (unsigned long)Wire.getClock());

  // ---- 2. Detección de 0x57 (solo se consulta esa dirección) ----
  uint8_t err = 0xFF;
  for (int i = 0; i < DETECT_ATTEMPTS && ok[1]; i++) {
    Wire.beginTransmission(MAX30102_ADDR);
    err = Wire.endTransmission();
    if (err == 0) break;
    delay(5);
  }
  ok[2] = (err == 0);
  Serial.printf("  2. Deteccion en 0x%02X: %s\n", MAX30102_ADDR,
                ok[2] ? "ACK recibido" : i2cErrorText(err));

  // ---- 3. PART_ID ----
  if (ok[2] && readRegister(REG_PART_ID, partId)) {
    ok[3] = (partId == EXPECTED_PART_ID);
    Serial.printf("  3. PART_ID (0xFF): 0x%02X (esperado 0x%02X)\n", partId, EXPECTED_PART_ID);
  } else {
    Serial.println("  3. PART_ID: no se pudo leer");
  }

  // ---- 4. REV_ID (informativo) ----
  if (ok[2]) revRead = readRegister(REG_REV_ID, revId);
  if (revRead) Serial.printf("  4. REV_ID (0xFE): 0x%02X (informativo)\n", revId);
  else         Serial.println("  4. REV_ID: no se pudo leer (informativo)");

  // ---- 5. 200 lecturas a 100 kHz ----
  if (ok[2]) {
    Serial.printf("  5. %d lecturas de PART_ID a 100 kHz\n", N_READS);
    ok[5] = readPartIdRepeated(I2C_100K);
  } else {
    Serial.println("  5. Omitida (sensor no detectado)");
  }

  // ---- 6. 200 lecturas a 400 kHz ----
  if (ok[2]) {
    Serial.printf("  6. %d lecturas de PART_ID a 400 kHz\n", N_READS);
    ok[6] = readPartIdRepeated(I2C_400K);
  } else {
    Serial.println("  6. Omitida (sensor no detectado)");
  }

  // ---- 7. Librería SparkFun ----
  // begin() reutiliza el bus ya iniciado, lo pone a 400 kHz y verifica
  // el PART_ID. No se llama a setup(): los LED permanecen apagados.
  if (ok[2]) {
    ok[7] = particleSensor.begin(Wire, I2C_SPEED_FAST);
    Serial.printf("  7. particleSensor.begin(Wire, 400 kHz): %s\n", ok[7] ? "OK" : "FALLO");
  } else {
    Serial.println("  7. Omitida (sensor no detectado)");
  }

  // ---- 8. Confirmación final con la librería ----
  if (ok[7]) {
    uint8_t id = particleSensor.readPartID();
    ok[8] = (id == EXPECTED_PART_ID);
    Serial.printf("  8. particleSensor.readPartID(): 0x%02X\n", id);
  } else {
    Serial.println("  8. Omitida (la libreria no se inicializo)");
  }

  // ---- Resumen ----
  Serial.println("\n------------------------------------------------");
  Serial.println(" RESUMEN");
  Serial.println("------------------------------------------------");
  printResult(1, "Bus I2C a 100 kHz en GPIO21/GPIO22", ok[1]);
  printResult(2, "Dispositivo detectado en 0x57", ok[2]);
  printResult(3, "PART_ID = 0x15", ok[3]);
  if (revRead) Serial.printf("  [INFO ] 4. REV_ID = 0x%02X (informativo)\n", revId);
  else         Serial.println("  [INFO ] 4. REV_ID no leido (informativo)");
  printResult(5, "200 lecturas sin errores a 100 kHz", ok[5]);
  printResult(6, "200 lecturas sin errores a 400 kHz", ok[6]);
  printResult(7, "particleSensor.begin()", ok[7]);
  printResult(8, "Confirmacion con readPartID()", ok[8]);
  Serial.println("------------------------------------------------");

  bool passed = ok[1] && ok[2] && ok[3] && ok[5] && ok[6] && ok[7] && ok[8];
  if (passed) {
    Serial.println(" RESULTADO: ETAPA 2.1 SUPERADA");
    Serial.println(" El MAX30102 se comunica correctamente por I2C a 100 y 400 kHz.");
  } else {
    Serial.println(" RESULTADO: ETAPA 2.1 NO SUPERADA");
    if (!ok[1]) Serial.println("  - El bus I2C no inicio: revisar placa seleccionada y pines.");
    if (ok[1] && !ok[2]) {
      Serial.println("  - Sin respuesta en 0x57: revisar VIN->3V3, GND, SDA->21, SCL->22");
      Serial.println("    (probar intercambiando SDA y SCL).");
    }
    if (ok[2] && !ok[3]) Serial.printf("  - PART_ID inesperado (0x%02X): posible modulo clon.\n", partId);
    if (ok[5] && !ok[6]) Serial.println("  - Falla solo a 400 kHz: acortar cables o usar 100 kHz.");
    if (ok[2] && !ok[5]) Serial.println("  - Errores a 100 kHz: posible falso contacto.");
    if (ok[2] && !ok[7]) Serial.println("  - La libreria no reconocio el sensor aunque responde por I2C.");
  }
  Serial.println("================================================");
  Serial.println("Verificacion periodica cada 5 s (mueva los cables suavemente).");
  Serial.println("Comandos: t = repetir pruebas, h = ayuda\n");

  monitorCount = 0;
  monitorErrors = 0;
  lastMonitorMs = millis();
}

// =================================================================
//  Verificación periódica (detecta falsos contactos)
// =================================================================
void periodicCheck(uint32_t nowMs) {
  if (!busStarted || nowMs - lastMonitorMs < MONITOR_PERIOD_MS) return;
  lastMonitorMs = nowMs;
  monitorCount++;
  uint8_t id = 0;
  bool ok = readRegister(REG_PART_ID, id) && id == EXPECTED_PART_ID;
  if (!ok) monitorErrors++;
  Serial.printf("[t=%lu s] Verificacion #%lu: %s | errores acumulados: %lu\n",
                (unsigned long)(nowMs / 1000UL), (unsigned long)monitorCount,
                ok ? "PART_ID 0x15 OK" : "SIN RESPUESTA", (unsigned long)monitorErrors);
}

void handleSerialCommands() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r' || c == '\n' || c == ' ') continue;
    if (c == 't' || c == 'T') {
      runTests();
    } else if (c == 'h' || c == 'H') {
      Serial.println("\nComandos: t = repetir pruebas, h = ayuda\n");
    } else {
      Serial.printf(">> Comando no reconocido: '%c'\n", c);
    }
  }
}

// =================================================================
void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(1000);
  runTests();
}

void loop() {
  handleSerialCommands();
  periodicCheck(millis());
}
