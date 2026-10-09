/*
 * =====================================================================
 *  ETAPA 3 - Adquisición y validación de las señales RED e IR
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
 *  Los valores mostrados son cuentas crudas del ADC del sensor, NO
 *  mediciones clínicas.
 *
 *  Conexiones (SIN TFT):
 *    MAX30102 VIN -> 3V3      MAX30102 SDA -> GPIO 21
 *    MAX30102 GND -> GND      MAX30102 SCL -> GPIO 22
 *    INT y RD sin conectar
 *
 *  Qué hace:
 *    - Configura el MAX30102 en modo RED + IR a 100 muestras/s efectivas.
 *    - Lee continuamente el FIFO del sensor (sin delays bloqueantes).
 *    - Muestra muestras y estadísticas por segundo.
 *    - Detecta saturación, valores en cero, pérdida de muestras y tasa
 *      de muestreo anómala.
 *    - Prueba guiada: sin dedo -> con dedo -> sin dedo.
 *
 *  NO calcula BPM, SpO2, ratio-of-ratios, ni hace filtrado o detección
 *  de picos. NO usa la TFT.
 *
 *  Monitor Serie a 115200 baudios. Comandos:
 *    d -> muestras decimadas (1 de cada 10) + resumen por segundo
 *    r -> solo resumen por segundo
 *    g -> todas las muestras para el Serial Plotter
 *    x -> todas las muestras en CSV (n,RED,IR) para exportar
 *    p -> prueba guiada del dedo
 *    + -> aumentar corriente de LED    - -> disminuir corriente de LED
 *    i -> mostrar configuración        h -> ayuda
 * =====================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include "MAX30105.h"

// =================================================================
//  CONFIGURACIÓN GENERAL
// =================================================================
const int      PIN_SDA              = 21;
const int      PIN_SCL              = 22;
const uint32_t I2C_FREQ             = 400000;   // validado en la Etapa 2.1
const uint16_t I2C_TIMEOUT_MS       = 50;
const uint32_t SERIAL_BAUD          = 115200;
const size_t   SERIAL_TX_BUFFER     = 2048;     // evita que imprimir bloquee

// ---- MAX30102 ----
const uint8_t  MAX30102_ADDR        = 0x57;
const uint8_t  EXPECTED_PART_ID     = 0x15;
const uint8_t  REG_FIFO_WR_PTR      = 0x04;     // 0x04 WR_PTR, 0x05 OVF, 0x06 RD_PTR
const uint8_t  REG_FIFO_DATA        = 0x07;
const uint8_t  REG_FIFO_CONFIG      = 0x08;
const uint8_t  REG_MODE_CONFIG      = 0x09;
const uint8_t  REG_SPO2_CONFIG      = 0x0A;
const uint8_t  REG_LED1_PA          = 0x0C;     // LED rojo
const uint8_t  REG_LED2_PA          = 0x0D;     // LED infrarrojo

// ---- Parámetros de adquisición ----
const uint8_t  LED_CURRENT_DEFAULT  = 0x1F;     // 0x1F * 0.2 mA = 6.2 mA (moderada)
const uint8_t  LED_CURRENT_MIN      = 0x05;     // 1.0 mA
const uint8_t  LED_CURRENT_MAX      = 0x50;     // 16.0 mA (límite para esta etapa)
const uint8_t  LED_CURRENT_STEP     = 0x05;     // 1.0 mA por paso
const byte     SAMPLE_AVERAGE       = 4;        // promedio interno del sensor
const byte     LED_MODE             = 2;        // 2 = RED + IR
const int      SAMPLE_RATE_INTERNAL = 400;      // 400 muestras/s internas
const int      PULSE_WIDTH_US       = 411;      // 411 us -> 18 bits
const int      ADC_RANGE_NA         = 4096;     // rango del ADC
const float    FS_NOMINAL           = 100.0f;   // 400 / 4 = 100 muestras/s efectivas

// Valores esperados en los registros tras la configuración
const uint8_t  EXPECTED_FIFO_CONFIG = 0x50;     // promedio 4 + rollover habilitado
const uint8_t  EXPECTED_MODE_CONFIG = 0x03;     // modo SpO2 (RED + IR)
const uint8_t  EXPECTED_SPO2_CONFIG = 0x2F;     // 4096 nA, 400 sps, 411 us

// ---- Lectura del FIFO ----
const uint32_t POLL_INTERVAL_MS     = 10;       // se vacía el FIFO cada 10 ms
const int      FIFO_DEPTH           = 32;       // capacidad del FIFO del sensor
const int      BYTES_PER_SAMPLE     = 6;        // 3 bytes RED + 3 bytes IR
const int      MAX_SAMPLES_PER_READ = 10;       // 60 bytes por transacción I2C

// ---- Umbrales experimentales (NO clínicos) ----
const uint32_t ADC_MAX_COUNTS       = 262143;   // 2^18 - 1
const uint32_t SATURATION_LEVEL     = 260000;   // cerca del máximo del ADC
const uint32_t FINGER_IR_THRESHOLD  = 50000;    // heurística: IR alto = dedo presente
const float    RATE_MIN             = 95.0f;
const float    RATE_MAX             = 105.0f;
const int      DECIMATION           = 10;       // modo 'd': 1 de cada 10 muestras

// ---- Prueba guiada del dedo ----
const uint32_t MIN_DELTA_IR         = 10000;    // aumento mínimo de IR con dedo
const uint32_t MIN_DELTA_RED        = 5000;     // aumento mínimo de RED con dedo
const float    MIN_RECOVERY         = 0.80f;    // debe recuperar >= 80 % del cambio

// =================================================================
//  ESTADO GLOBAL
// =================================================================
MAX30105 particleSensor;

bool     sensorReady      = false;
uint8_t  ledCurrent       = LED_CURRENT_DEFAULT;
char     outputMode       = 'd';
uint32_t lastPollMs       = 0;
uint32_t lastRetryMs      = 0;

// ---- Contadores acumulados (desde el arranque) ----
uint32_t sampleIndex      = 0;       // número de muestra
uint32_t lostSamples      = 0;       // muestras perdidas según OVF_COUNTER
uint32_t overflowEvents   = 0;
uint32_t i2cErrors        = 0;
int      lastFifoLevel    = 0;

// ---- Estadísticas de la ventana de 1 segundo ----
uint32_t winStartMs       = 0;
uint32_t winCount         = 0;
uint64_t winSumRed        = 0;
uint64_t winSumIr         = 0;
uint32_t winMinRed        = 0;
uint32_t winMaxRed        = 0;
uint32_t winMinIr         = 0;
uint32_t winMaxIr         = 0;
uint32_t winSaturated     = 0;
uint32_t winZeros         = 0;
int      winMaxFifo       = 0;
uint32_t winLostStart     = 0;
uint32_t winI2cErrStart   = 0;

// ---- Prueba guiada: fases ----
const int TEST_IDLE             = 0;
const int TEST_BASELINE_WAIT    = 1;
const int TEST_BASELINE_MEASURE = 2;
const int TEST_FINGER_WAIT      = 3;
const int TEST_FINGER_MEASURE   = 4;
const int TEST_REMOVE_WAIT      = 5;
const int TEST_REMOVE_MEASURE   = 6;

const uint32_t TEST_WAIT_SHORT_MS = 3000;
const uint32_t TEST_WAIT_FINGER_MS = 5000;
const uint32_t TEST_MEASURE_MS     = 5000;

int      testPhase         = TEST_IDLE;
uint32_t testPhaseStartMs  = 0;
uint32_t testStartMs       = 0;
uint32_t testStartSamples  = 0;
uint32_t testStartLost     = 0;
char     modeBeforeTest    = 'd';

// Acumuladores por bloque de medición: [0] sin dedo, [1] con dedo, [2] retirado
uint64_t testSumRed[3];
uint64_t testSumIr[3];
uint32_t testCount[3];
uint32_t testSaturated[3];
uint32_t testMinIr[3];
uint32_t testMaxIr[3];

// =================================================================
//  MÓDULO: ACCESO I2C DE BAJO NIVEL
// =================================================================
// Lee 'len' registros consecutivos a partir de 'reg'.
bool readRegisters(uint8_t reg, uint8_t *buffer, int len) {
  Wire.beginTransmission(MAX30102_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(MAX30102_ADDR, (size_t)len) != (size_t)len) return false;
  for (int i = 0; i < len; i++) {
    if (Wire.available() < 1) return false;
    buffer[i] = (uint8_t)Wire.read();
  }
  return true;
}

bool readRegister(uint8_t reg, uint8_t &value) {
  return readRegisters(reg, &value, 1);
}

// =================================================================
//  MÓDULO: SENSOR MAX30102 (inicialización y configuración)
// =================================================================
void printRegisterCheck(const char *name, uint8_t reg, uint8_t expected) {
  uint8_t v = 0;
  if (readRegister(reg, v)) {
    Serial.printf("    %-12s (0x%02X): 0x%02X  esperado 0x%02X  [%s]\n",
                  name, reg, v, expected, (v == expected) ? " OK " : "DIFERENTE");
  } else {
    Serial.printf("    %-12s (0x%02X): ERROR DE LECTURA\n", name, reg);
  }
}

void printConfiguration() {
  Serial.println("\n  Configuracion de adquisicion:");
  Serial.println("    Modo               : RED + IR (modo SpO2 del sensor)");
  Serial.printf ("    Muestreo interno   : %d muestras/s, promedio de %d\n",
                 SAMPLE_RATE_INTERNAL, (int)SAMPLE_AVERAGE);
  Serial.printf ("    Muestreo efectivo  : %.0f muestras/s (nominal)\n", FS_NOMINAL);
  Serial.printf ("    Ancho de pulso     : %d us (resolucion 18 bits, 0-%lu)\n",
                 PULSE_WIDTH_US, (unsigned long)ADC_MAX_COUNTS);
  Serial.printf ("    Rango del ADC      : %d nA\n", ADC_RANGE_NA);
  Serial.printf ("    Corriente LED      : 0x%02X = %.1f mA (RED e IR)\n",
                 ledCurrent, ledCurrent * 0.2f);
  Serial.printf ("    I2C                : %lu Hz, SDA=%d, SCL=%d\n",
                 (unsigned long)I2C_FREQ, PIN_SDA, PIN_SCL);
  Serial.println("  Registros leidos del sensor:");
  printRegisterCheck("FIFO_CONFIG", REG_FIFO_CONFIG, EXPECTED_FIFO_CONFIG);
  printRegisterCheck("MODE_CONFIG", REG_MODE_CONFIG, EXPECTED_MODE_CONFIG);
  printRegisterCheck("SPO2_CONFIG", REG_SPO2_CONFIG, EXPECTED_SPO2_CONFIG);
  printRegisterCheck("LED1_PA RED", REG_LED1_PA, ledCurrent);
  printRegisterCheck("LED2_PA IR",  REG_LED2_PA, ledCurrent);
  Serial.println();
}

bool initSensor() {
  Wire.begin(PIN_SDA, PIN_SCL, I2C_FREQ);
  Wire.setTimeOut(I2C_TIMEOUT_MS);

  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("  [FALLA] particleSensor.begin(): el MAX30102 no responde.");
    Serial.println("          Revisar VIN->3V3, GND, SDA->21, SCL->22.");
    return false;
  }
  uint8_t id = particleSensor.readPartID();
  if (id != EXPECTED_PART_ID) {
    Serial.printf("  [FALLA] PART_ID = 0x%02X (esperado 0x%02X)\n", id, EXPECTED_PART_ID);
    return false;
  }
  Serial.printf("  [ OK ] MAX30102 detectado en 0x%02X, PART_ID 0x%02X\n", MAX30102_ADDR, id);

  // setup() de la librería: reinicio por software, promedio, rollover
  // del FIFO, modo RED+IR, rango ADC, muestreo, ancho de pulso,
  // corriente de LED, asignación de ranuras y limpieza del FIFO.
  particleSensor.setup(ledCurrent, SAMPLE_AVERAGE, LED_MODE,
                       SAMPLE_RATE_INTERNAL, PULSE_WIDTH_US, ADC_RANGE_NA);
  Serial.println("  [ OK ] Sensor configurado (LED encendidos a corriente moderada)");
  printConfiguration();
  return true;
}

void setLedCurrent(uint8_t value) {
  ledCurrent = value;
  particleSensor.setPulseAmplitudeRed(ledCurrent);
  particleSensor.setPulseAmplitudeIR(ledCurrent);
  Serial.printf(">> Corriente de LED: 0x%02X = %.1f mA\n", ledCurrent, ledCurrent * 0.2f);
}

// =================================================================
//  MÓDULO: ESTADÍSTICAS POR SEGUNDO
// =================================================================
void resetWindow(uint32_t nowMs) {
  winStartMs     = nowMs;
  winCount       = 0;
  winSumRed      = 0;
  winSumIr       = 0;
  winMinRed      = ADC_MAX_COUNTS;
  winMaxRed      = 0;
  winMinIr       = ADC_MAX_COUNTS;
  winMaxIr       = 0;
  winSaturated   = 0;
  winZeros       = 0;
  winMaxFifo     = 0;
  winLostStart   = lostSamples;
  winI2cErrStart = i2cErrors;
}

void accumulateWindow(uint32_t red, uint32_t ir, int fifoLevel) {
  winCount++;
  winSumRed += red;
  winSumIr  += ir;
  if (red < winMinRed) winMinRed = red;
  if (red > winMaxRed) winMaxRed = red;
  if (ir  < winMinIr)  winMinIr  = ir;
  if (ir  > winMaxIr)  winMaxIr  = ir;
  if (red >= SATURATION_LEVEL || ir >= SATURATION_LEVEL) winSaturated++;
  if (red == 0 || ir == 0) winZeros++;
  if (fifoLevel > winMaxFifo) winMaxFifo = fifoLevel;
}

void reportWindow(uint32_t nowMs) {
  uint32_t elapsed = nowMs - winStartMs;
  if (elapsed < 1000) return;

  float rate = (elapsed > 0) ? (winCount * 1000.0f / elapsed) : 0.0f;
  uint32_t meanRed = (winCount > 0) ? (uint32_t)(winSumRed / winCount) : 0;
  uint32_t meanIr  = (winCount > 0) ? (uint32_t)(winSumIr / winCount) : 0;
  uint32_t lostNow = lostSamples - winLostStart;
  uint32_t errNow  = i2cErrors - winI2cErrStart;
  bool finger      = (meanIr >= FINGER_IR_THRESHOLD);

  if (outputMode == 'd' || outputMode == 'r') {
    if (winCount == 0) {
      Serial.printf("== t=%lus | SIN MUESTRAS en el ultimo segundo | errores I2C: %lu\n",
                    (unsigned long)(nowMs / 1000UL), (unsigned long)errNow);
    } else {
      Serial.printf("== t=%lus | %.1f muestras/s | RED media %lu [%lu-%lu] | "
                    "IR media %lu [%lu-%lu] | FIFO max %d | %s | alertas:",
                    (unsigned long)(nowMs / 1000UL), rate,
                    (unsigned long)meanRed, (unsigned long)winMinRed, (unsigned long)winMaxRed,
                    (unsigned long)meanIr, (unsigned long)winMinIr, (unsigned long)winMaxIr,
                    winMaxFifo, finger ? "DEDO" : "SIN DEDO");
      bool anyAlert = false;
      if (winSaturated > 0) { Serial.printf(" SATURACION(%lu)", (unsigned long)winSaturated); anyAlert = true; }
      if (winZeros > 0)     { Serial.printf(" CEROS(%lu)", (unsigned long)winZeros); anyAlert = true; }
      if (lostNow > 0)      { Serial.printf(" PERDIDAS(%lu)", (unsigned long)lostNow); anyAlert = true; }
      if (errNow > 0)       { Serial.printf(" ERR_I2C(%lu)", (unsigned long)errNow); anyAlert = true; }
      if (rate < RATE_MIN || rate > RATE_MAX) { Serial.print(" TASA"); anyAlert = true; }
      if (!anyAlert) Serial.print(" ninguna");
      Serial.println();
    }
  }
  resetWindow(nowMs);
}

// =================================================================
//  MÓDULO: PRUEBA GUIADA DEL DEDO
// =================================================================
void resetTestAccumulators() {
  for (int k = 0; k < 3; k++) {
    testSumRed[k] = 0;
    testSumIr[k] = 0;
    testCount[k] = 0;
    testSaturated[k] = 0;
    testMinIr[k] = ADC_MAX_COUNTS;
    testMaxIr[k] = 0;
  }
}

// Índice del bloque de medición activo (-1 si no se está midiendo)
int activeTestBlock() {
  if (testPhase == TEST_BASELINE_MEASURE) return 0;
  if (testPhase == TEST_FINGER_MEASURE)   return 1;
  if (testPhase == TEST_REMOVE_MEASURE)   return 2;
  return -1;
}

void accumulateTest(uint32_t red, uint32_t ir) {
  int k = activeTestBlock();
  if (k < 0) return;
  testSumRed[k] += red;
  testSumIr[k]  += ir;
  testCount[k]++;
  if (red >= SATURATION_LEVEL || ir >= SATURATION_LEVEL) testSaturated[k]++;
  if (ir < testMinIr[k]) testMinIr[k] = ir;
  if (ir > testMaxIr[k]) testMaxIr[k] = ir;
}

uint32_t testMeanRed(int k) {
  return (testCount[k] > 0) ? (uint32_t)(testSumRed[k] / testCount[k]) : 0;
}

uint32_t testMeanIr(int k) {
  return (testCount[k] > 0) ? (uint32_t)(testSumIr[k] / testCount[k]) : 0;
}

// Fracción del cambio que se recuperó al retirar el dedo (1.0 = volvió al fondo)
float recoveryFraction(uint32_t base, uint32_t finger, uint32_t removed) {
  float delta = (float)finger - (float)base;
  if (delta <= 0.0f) return 0.0f;
  return ((float)finger - (float)removed) / delta;
}

void printTestResult(const char *name, bool ok) {
  Serial.printf("  [%s] %s\n", ok ? " OK " : "FALLA", name);
}

void startFingerTest(uint32_t nowMs) {
  if (!sensorReady) {
    Serial.println(">> El sensor no esta listo; no se puede iniciar la prueba.");
    return;
  }
  resetTestAccumulators();
  modeBeforeTest   = outputMode;
  outputMode       = 'r';              // salida legible durante la prueba
  testStartMs      = nowMs;
  testStartSamples = sampleIndex;
  testStartLost    = lostSamples;
  testPhase        = TEST_BASELINE_WAIT;
  testPhaseStartMs = nowMs;

  Serial.println("\n================================================");
  Serial.println(" PRUEBA GUIADA RED/IR (duracion aprox. 26 s)");
  Serial.println(" Salida en modo resumen durante la prueba.");
  Serial.println("================================================");
  Serial.println(">> FASE 1: RETIRE el dedo del sensor y no lo toque.");
  Serial.println("   (3 s de espera + 5 s de medicion del fondo)");
}

void finishFingerTest(uint32_t nowMs) {
  uint32_t red0 = testMeanRed(0), ir0 = testMeanIr(0);
  uint32_t red1 = testMeanRed(1), ir1 = testMeanIr(1);
  uint32_t red2 = testMeanRed(2), ir2 = testMeanIr(2);

  long dRed = (long)red1 - (long)red0;
  long dIr  = (long)ir1  - (long)ir0;
  float recRed = recoveryFraction(red0, red1, red2);
  float recIr  = recoveryFraction(ir0, ir1, ir2);

  uint32_t testSamples = sampleIndex - testStartSamples;
  uint32_t testLost    = lostSamples - testStartLost;
  float    testRate    = testSamples * 1000.0f / (float)(nowMs - testStartMs);
  uint32_t satTotal    = testSaturated[0] + testSaturated[1] + testSaturated[2];

  bool okIr   = (dIr  >= (long)MIN_DELTA_IR);
  bool okRed  = (dRed >= (long)MIN_DELTA_RED);
  bool okRec  = okIr && okRed && (recIr >= MIN_RECOVERY) && (recRed >= MIN_RECOVERY);
  bool okSat  = (satTotal == 0);
  bool okLost = (testLost == 0);
  bool okRate = (testRate >= RATE_MIN && testRate <= RATE_MAX);

  Serial.println("\n================================================");
  Serial.println(" RESULTADOS DE LA PRUEBA GUIADA (valores crudos del ADC)");
  Serial.println("================================================");
  Serial.println("  Bloque          RED media    IR media   IR [min-max]   muestras");
  Serial.printf ("  1. Sin dedo    %9lu  %10lu   [%lu-%lu]   %lu\n",
                 (unsigned long)red0, (unsigned long)ir0,
                 (unsigned long)testMinIr[0], (unsigned long)testMaxIr[0], (unsigned long)testCount[0]);
  Serial.printf ("  2. Con dedo    %9lu  %10lu   [%lu-%lu]   %lu\n",
                 (unsigned long)red1, (unsigned long)ir1,
                 (unsigned long)testMinIr[1], (unsigned long)testMaxIr[1], (unsigned long)testCount[1]);
  Serial.printf ("  3. Retirado    %9lu  %10lu   [%lu-%lu]   %lu\n",
                 (unsigned long)red2, (unsigned long)ir2,
                 (unsigned long)testMinIr[2], (unsigned long)testMaxIr[2], (unsigned long)testCount[2]);
  Serial.println("------------------------------------------------");
  Serial.printf ("  Cambio con dedo : RED %+ld cuentas, IR %+ld cuentas\n", dRed, dIr);
  Serial.printf ("  Recuperacion    : RED %.0f %%, IR %.0f %% (1.0 = volvio al fondo)\n",
                 recRed * 100.0f, recIr * 100.0f);
  Serial.printf ("  Tasa de muestreo: %.1f muestras/s | perdidas: %lu | saturadas: %lu\n",
                 testRate, (unsigned long)testLost, (unsigned long)satTotal);
  Serial.println("------------------------------------------------");
  printTestResult("IR aumenta claramente con el dedo (>= 10000 cuentas)", okIr);
  printTestResult("RED aumenta claramente con el dedo (>= 5000 cuentas)", okRed);
  printTestResult("Al retirar el dedo se recupera >= 80 % del cambio", okRec);
  printTestResult("Sin saturacion del ADC", okSat);
  printTestResult("Sin muestras perdidas (FIFO)", okLost);
  printTestResult("Tasa de muestreo entre 95 y 105 muestras/s", okRate);
  Serial.println("------------------------------------------------");

  if (okIr && okRed && okRec && okSat && okLost && okRate) {
    Serial.println(" RESULTADO: PRUEBA RED/IR SUPERADA");
    Serial.println(" Las senales RED e IR se adquieren y responden al dedo.");
  } else {
    Serial.println(" RESULTADO: PRUEBA RED/IR NO SUPERADA. Sugerencias:");
    if (!okIr || !okRed) {
      Serial.println("  - Cubra por completo la ventana del sensor con la yema del dedo.");
      Serial.println("  - Si el cambio es pequeno, aumente la corriente con '+'.");
    }
    if (!okRec) Serial.println("  - Retire el dedo por completo y evite luz directa sobre el sensor.");
    if (!okSat) Serial.println("  - Hay saturacion: reduzca la corriente con '-' o presione menos.");
    if (!okLost) Serial.println("  - Hubo perdidas: use el modo 'r' o 'd' (menos texto por segundo).");
    if (!okRate) Serial.println("  - Tasa anomala: revise errores I2C en los resumenes.");
  }
  Serial.println(" (Prueba experimental. No es una medicion clinica.)");
  Serial.println("================================================\n");

  testPhase  = TEST_IDLE;
  outputMode = modeBeforeTest;
}

void updateFingerTest(uint32_t nowMs) {
  if (testPhase == TEST_IDLE) return;
  uint32_t elapsed = nowMs - testPhaseStartMs;

  if (testPhase == TEST_BASELINE_WAIT && elapsed >= TEST_WAIT_SHORT_MS) {
    testPhase = TEST_BASELINE_MEASURE;
    testPhaseStartMs = nowMs;
    Serial.println(">> Midiendo fondo SIN dedo (5 s)...");
  } else if (testPhase == TEST_BASELINE_MEASURE && elapsed >= TEST_MEASURE_MS) {
    testPhase = TEST_FINGER_WAIT;
    testPhaseStartMs = nowMs;
    Serial.println(">> FASE 2: COLOQUE la yema del dedo sobre el sensor, sin presionar.");
    Serial.println("   Mantengalo quieto. (5 s de estabilizacion + 5 s de medicion)");
  } else if (testPhase == TEST_FINGER_WAIT && elapsed >= TEST_WAIT_FINGER_MS) {
    testPhase = TEST_FINGER_MEASURE;
    testPhaseStartMs = nowMs;
    Serial.println(">> Midiendo CON dedo (5 s)... no lo mueva.");
  } else if (testPhase == TEST_FINGER_MEASURE && elapsed >= TEST_MEASURE_MS) {
    testPhase = TEST_REMOVE_WAIT;
    testPhaseStartMs = nowMs;
    Serial.println(">> FASE 3: RETIRE el dedo del sensor.");
    Serial.println("   (3 s de espera + 5 s de medicion)");
  } else if (testPhase == TEST_REMOVE_WAIT && elapsed >= TEST_WAIT_SHORT_MS) {
    testPhase = TEST_REMOVE_MEASURE;
    testPhaseStartMs = nowMs;
    Serial.println(">> Midiendo SIN dedo nuevamente (5 s)...");
  } else if (testPhase == TEST_REMOVE_MEASURE && elapsed >= TEST_MEASURE_MS) {
    finishFingerTest(nowMs);
  }
}

// =================================================================
//  MÓDULO: PROCESAMIENTO DE CADA MUESTRA (sin procesamiento de señal)
// =================================================================
void processSample(uint32_t red, uint32_t ir, int fifoLevel) {
  sampleIndex++;
  accumulateWindow(red, ir, fifoLevel);
  accumulateTest(red, ir);

  if (outputMode == 'd') {
    if (sampleIndex % DECIMATION == 0) {
      Serial.printf("#%lu  RED=%6lu  IR=%6lu  FIFO=%d\n",
                    (unsigned long)sampleIndex, (unsigned long)red,
                    (unsigned long)ir, fifoLevel);
    }
  } else if (outputMode == 'g') {
    Serial.printf("RED:%lu,IR:%lu\n", (unsigned long)red, (unsigned long)ir);
  } else if (outputMode == 'x') {
    Serial.printf("%lu,%lu,%lu\n", (unsigned long)sampleIndex,
                  (unsigned long)red, (unsigned long)ir);
  }
}

// =================================================================
//  MÓDULO: LECTURA DEL FIFO DEL MAX30102
// =================================================================
// 1. Lee WR_PTR (0x04), OVF_COUNTER (0x05) y RD_PTR (0x06) en una sola
//    transacción.
// 2. Muestras pendientes = WR_PTR - RD_PTR (módulo 32).
//    Si OVF_COUNTER > 0, el FIFO se desbordó: se considera que contiene
//    32 muestras válidas pendientes (FIFO lleno) y que se perdieron
//    OVF_COUNTER muestras.
//    OVF_COUNTER es un registro de 5 bits: satura en 0x1F (31). Si se
//    pierden más de 31 muestras entre dos lecturas, solo se contabilizan
//    31, por lo que lostSamples es un valor MÍNIMO de muestras perdidas.
//    El contador se reinicia a 0 al extraer una muestra completa del FIFO.
// 3. Lee los datos en ráfagas desde FIFO_DATA (0x07). Cada muestra son
//    6 bytes: RED (3 bytes) + IR (3 bytes), 18 bits útiles cada uno.
// Devuelve el número de muestras leídas, o -1 si hubo error de I2C.
int readFifo() {
  uint8_t ptr[3];
  if (!readRegisters(REG_FIFO_WR_PTR, ptr, 3)) {
    i2cErrors++;
    return -1;
  }
  int writePtr = ptr[0] & 0x1F;
  int overflow = ptr[1] & 0x1F;   // OVF_COUNTER: 5 bits, satura en 0x1F
  int readPtr  = ptr[2] & 0x1F;

  int pending = (writePtr - readPtr) & 0x1F;
  if (overflow > 0) {
    // Desbordamiento: el FIFO está lleno con 32 muestras válidas.
    overflowEvents++;
    lostSamples += (uint32_t)overflow;   // mínimo perdido (puede estar saturado)
    pending = FIFO_DEPTH;
  }
  lastFifoLevel = pending;

  int total = 0;
  uint8_t data[MAX_SAMPLES_PER_READ * BYTES_PER_SAMPLE];

  while (pending > 0) {
    int n = (pending > MAX_SAMPLES_PER_READ) ? MAX_SAMPLES_PER_READ : pending;
    if (!readRegisters(REG_FIFO_DATA, data, n * BYTES_PER_SAMPLE)) {
      i2cErrors++;
      return (total > 0) ? total : -1;
    }
    for (int i = 0; i < n; i++) {
      int b = i * BYTES_PER_SAMPLE;
      uint32_t red = (((uint32_t)data[b]     << 16) | ((uint32_t)data[b + 1] << 8) | data[b + 2]) & 0x3FFFF;
      uint32_t ir  = (((uint32_t)data[b + 3] << 16) | ((uint32_t)data[b + 4] << 8) | data[b + 5]) & 0x3FFFF;
      processSample(red, ir, lastFifoLevel);
    }
    pending -= n;
    total   += n;
  }
  return total;
}

// =================================================================
//  MÓDULO: INTERFAZ POR MONITOR SERIE
// =================================================================
void printHelp() {
  Serial.println("\nComandos:");
  Serial.println("  d -> muestras decimadas (1 de cada 10) + resumen por segundo");
  Serial.println("  r -> solo resumen por segundo");
  Serial.println("  g -> todas las muestras para el Serial Plotter (RED e IR)");
  Serial.println("  x -> todas las muestras en CSV: n,RED,IR");
  Serial.println("  p -> prueba guiada del dedo (aprox. 26 s)");
  Serial.println("  + -> aumentar corriente de LED (1 mA)");
  Serial.println("  - -> disminuir corriente de LED (1 mA)");
  Serial.println("  i -> mostrar configuracion y registros");
  Serial.println("  h -> ayuda\n");
}

void setOutputMode(char mode) {
  if (testPhase != TEST_IDLE) {
    Serial.println(">> Prueba en curso: el modo se restablecera al terminar.");
    modeBeforeTest = mode;
    return;
  }
  outputMode = mode;
  if (mode == 'd') Serial.println(">> Modo: muestras decimadas + resumen");
  if (mode == 'r') Serial.println(">> Modo: solo resumen por segundo");
  if (mode == 'x') Serial.println("n,RED,IR");   // encabezado CSV
  // En modo 'g' no se imprime texto extra para no alterar el Serial Plotter
}

void handleSerialCommands(uint32_t nowMs) {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r' || c == '\n' || c == ' ') continue;

    if (c == 'd' || c == 'D') {
      setOutputMode('d');
    } else if (c == 'r' || c == 'R') {
      setOutputMode('r');
    } else if (c == 'g' || c == 'G') {
      setOutputMode('g');
    } else if (c == 'x' || c == 'X') {
      setOutputMode('x');
    } else if (c == 'p' || c == 'P') {
      if (testPhase == TEST_IDLE) startFingerTest(nowMs);
      else Serial.println(">> Ya hay una prueba en curso.");
    } else if (c == '+') {
      if (testPhase != TEST_IDLE) Serial.println(">> No se cambia la corriente durante la prueba.");
      else if (ledCurrent + LED_CURRENT_STEP <= LED_CURRENT_MAX) setLedCurrent(ledCurrent + LED_CURRENT_STEP);
      else Serial.println(">> Corriente maxima de esta etapa alcanzada (16 mA).");
    } else if (c == '-') {
      if (testPhase != TEST_IDLE) Serial.println(">> No se cambia la corriente durante la prueba.");
      else if (ledCurrent >= LED_CURRENT_MIN + LED_CURRENT_STEP) setLedCurrent(ledCurrent - LED_CURRENT_STEP);
      else Serial.println(">> Corriente minima alcanzada (1 mA).");
    } else if (c == 'i' || c == 'I') {
      if (sensorReady) printConfiguration();
      else Serial.println(">> Sensor no inicializado.");
    } else if (c == 'h' || c == 'H') {
      printHelp();
    } else {
      Serial.printf(">> Comando no reconocido: '%c' (escriba 'h')\n", c);
    }
  }
}

// =================================================================
//  SETUP Y LOOP
// =================================================================
void setup() {
  Serial.setTxBufferSize(SERIAL_TX_BUFFER);   // debe llamarse antes de begin()
  Serial.begin(SERIAL_BAUD);
  delay(1000);                                // solo en setup

  Serial.println();
  Serial.println("================================================");
  Serial.println(" ETAPA 3 - Adquisicion de senales RED e IR");
  Serial.println(" Prototipo academico - NO es dispositivo medico");
  Serial.println(" Valores = cuentas crudas del ADC, no mediciones clinicas");
  Serial.println("================================================");

  sensorReady = initSensor();
  if (sensorReady) {
    Serial.println("Adquisicion iniciada. Escriba 'p' para la prueba guiada, 'h' para ayuda.\n");
  } else {
    Serial.println("Se reintentara la inicializacion cada 5 s.\n");
  }
  uint32_t now = millis();
  lastPollMs  = now;
  lastRetryMs = now;
  resetWindow(now);
}

void loop() {
  uint32_t now = millis();

  if (!sensorReady) {
    if (now - lastRetryMs >= 5000) {
      lastRetryMs = now;
      Serial.println("Reintentando inicializacion del MAX30102...");
      sensorReady = initSensor();
      resetWindow(millis());
    }
    handleSerialCommands(now);
    return;
  }

  // 1) Adquisición: vaciar el FIFO del sensor cada POLL_INTERVAL_MS
  if (now - lastPollMs >= POLL_INTERVAL_MS) {
    lastPollMs = now;
    readFifo();
  }

  // 2) Comandos del usuario
  handleSerialCommands(now);

  // 3) Prueba guiada (máquina de estados no bloqueante)
  updateFingerTest(now);

  // 4) Resumen de cada segundo
  reportWindow(now);
}
