/*
 * =====================================================================
 *  ETAPA 4.1 - Procesamiento de la señal PPG (RED e IR)
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
 *  Esta etapa NO calcula frecuencia cardiaca ni SpO2.
 *
 *  Conexiones (SIN TFT):
 *    MAX30102 VIN -> 3V3      MAX30102 SDA -> GPIO 21
 *    MAX30102 GND -> GND      MAX30102 SCL -> GPIO 22
 *    INT y RD sin conectar
 *
 *  Adquisición: IDÉNTICA a la Etapa 3 validada (FIFO leído con Wire,
 *  400 kHz, promedio 4, 400 sps internos -> ~100 muestras/s efectivas).
 *
 *  Procesamiento por canal (RED e IR), en tiempo real:
 *    x ──► Estimador DC (paso bajo exponencial, tau = 1 s) ──► DC
 *    x - DC ──► Pasa-altas Butterworth 2.º orden 0.5 Hz
 *           ──► Pasa-bajas Butterworth 2.º orden 5 Hz ──► AC
 *    PPG = -AC  (invertida: el pico sistólico queda hacia arriba)
 *
 *  NO se implementa: detección de picos, BPM, SpO2, ratio-of-ratios,
 *  TFT, WiFi, Bluetooth ni alarmas.
 *
 *  Cambios de la Etapa 4.1 respecto a la Etapa 4:
 *    - Periodo refractario: sin nuevos reinicios durante los 2 s de
 *      estabilización (una rampa de dedo = un solo reinicio).
 *    - Estabilización en dos fases: 1 s de DC rápido (tau = 0.1 s) con
 *      el pasa-banda detenido + 1 s de calentamiento de los filtros.
 *    - Reinicios clasificados al final de la estabilización:
 *      dedo colocado/retirado (fingerResets) o artefacto (artifactResets).
 *    - Prueba guiada: ventanas de 2 s, descarte de ventanas con reinicio,
 *      reinicios contados por bloque y solo durante la prueba, IR crudo
 *      por bloque y recuperación del DC IR y del IR crudo.
 *
 *  Monitor Serie a 115200 baudios. Comandos:
 *    d -> diagnóstico por segundo (DC, AC p-p, AC RMS, AC/DC)
 *    p -> procesamiento: crudo, DC y AC de RED e IR (1 de cada 10)
 *    g -> Serial Plotter: PPG de IR y RED (todas las muestras)
 *    x -> CSV: n,RED,IR,RED_DC,IR_DC,RED_AC,IR_AC
 *    t -> prueba guiada de la señal PPG (~31 s)
 *    r -> reiniciar filtros
 *    + / - -> corriente de LED
 *    i -> configuración y coeficientes     h -> ayuda
 * =====================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include "MAX30105.h"

// =================================================================
//  CONFIGURACIÓN GENERAL (sin cambios respecto a la Etapa 3)
// =================================================================
const int      PIN_SDA              = 21;
const int      PIN_SCL              = 22;
const uint32_t I2C_FREQ             = 400000;
const uint16_t I2C_TIMEOUT_MS       = 50;
const uint32_t SERIAL_BAUD          = 115200;
const size_t   SERIAL_TX_BUFFER     = 2048;

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
const uint8_t  LED_CURRENT_DEFAULT  = 0x1F;     // 0x1F * 0.2 mA = 6.2 mA
const uint8_t  LED_CURRENT_MIN      = 0x05;     // 1.0 mA
const uint8_t  LED_CURRENT_MAX      = 0x50;     // 16.0 mA
const uint8_t  LED_CURRENT_STEP     = 0x05;     // 1.0 mA por paso
const byte     SAMPLE_AVERAGE       = 4;
const byte     LED_MODE             = 2;        // RED + IR
const int      SAMPLE_RATE_INTERNAL = 400;
const int      PULSE_WIDTH_US       = 411;
const int      ADC_RANGE_NA         = 4096;
const float    FS_NOMINAL           = 100.0f;   // 400 / 4

const uint8_t  EXPECTED_FIFO_CONFIG = 0x50;
const uint8_t  EXPECTED_MODE_CONFIG = 0x03;
const uint8_t  EXPECTED_SPO2_CONFIG = 0x2F;

// ---- Lectura del FIFO ----
const uint32_t POLL_INTERVAL_MS     = 10;
const int      FIFO_DEPTH           = 32;
const int      BYTES_PER_SAMPLE     = 6;
const int      MAX_SAMPLES_PER_READ = 10;

// ---- Umbrales de adquisición (NO clínicos) ----
const uint32_t ADC_MAX_COUNTS       = 262143;
const uint32_t SATURATION_LEVEL     = 260000;
const uint32_t FINGER_IR_THRESHOLD  = 50000;    // heurística de presencia de dedo
const float    RATE_MIN             = 95.0f;
const float    RATE_MAX             = 105.0f;

// =================================================================
//  CONFIGURACIÓN DEL PROCESAMIENTO PPG (nuevo en la Etapa 4)
// =================================================================
const int      CH_RED               = 0;        // índice de canal
const int      CH_IR                = 1;
const float    DC_TIME_CONSTANT_S   = 1.0f;     // estimador DC: tau = 1 s
const float    HP_CUTOFF_HZ         = 0.5f;     // pasa-altas (30 lpm)
const float    LP_CUTOFF_HZ         = 5.0f;     // pasa-bajas
const float    BUTTERWORTH_Q        = 0.70710678f;
const float    STEP_RELATIVE        = 0.10f;    // cambio brusco: >10 % del DC...
const float    STEP_MIN_COUNTS      = 5000.0f;  // ...y al menos 5000 cuentas
const uint32_t SETTLE_SAMPLES       = 200;      // 2 s de estabilización tras reinicio
const float    SETTLE_DC_TIME_CONSTANT_S = 0.1f; // DC rápido durante la estabilización
const uint32_t SETTLE_FAST_DC_SAMPLES = 100;    // 1.er segundo: solo DC rápido, filtros detenidos
const int      DECIMATION           = 10;       // modo 'p': 1 de cada 10

// ---- Criterios de la prueba guiada (experimentales, ajustables) ----
const float    MIN_DC_DELTA_IR      = 10000.0f; // aumento de DC IR con dedo
const float    MIN_AC_RATIO         = 3.0f;     // AC RMS con dedo / AC RMS sin dedo
const float    MIN_PP_IR            = 100.0f;   // AC p-p medio IR (cuentas)
const float    MIN_PP_RED           = 50.0f;    // AC p-p medio RED (cuentas)
const float    MIN_ACDC_PERCENT     = 0.05f;    // AC/DC IR mínimo (%)
const float    FLAT_PP_LIMIT        = 20.0f;    // p-p mínimo en CADA ventana de 1 s
const float    MAX_PP_CV            = 0.50f;    // variación máxima de amplitud
const float    MIN_RECOVERY         = 0.80f;    // recuperación del DC al retirar
const float    MIN_RED_IR_CORR      = 0.70f;    // correlación AC RED-IR mínima

// =================================================================
//  ESTADO GLOBAL
// =================================================================
MAX30105 particleSensor;

bool     sensorReady      = false;
uint8_t  ledCurrent       = LED_CURRENT_DEFAULT;
char     outputMode       = 'd';
uint32_t lastPollMs       = 0;
uint32_t lastRetryMs      = 0;

// ---- Contadores de adquisición ----
uint32_t sampleIndex      = 0;
uint32_t lostSamples      = 0;
uint32_t overflowEvents   = 0;
uint32_t i2cErrors        = 0;
int      lastFifoLevel    = 0;

// ---- Coeficientes de los filtros (se calculan en setup) ----
float dcAlpha = 0.0f;
float dcAlphaFast = 0.0f;          // alfa del DC durante la estabilización
float hpB0 = 0, hpB1 = 0, hpB2 = 0, hpA1 = 0, hpA2 = 0;
float lpB0 = 0, lpB1 = 0, lpB2 = 0, lpA1 = 0, lpA2 = 0;

// ---- Estado de los filtros por canal: [0] RED, [1] IR ----
float    dcEstimate[2];            // componente DC
float    hpZ1[2], hpZ2[2];         // estado del pasa-altas (DF2T)
float    lpZ1[2], lpZ2[2];         // estado del pasa-bajas (DF2T)
float    lastDc[2];                // último DC
float    lastAc[2];                // último AC filtrado
bool     filtersPrimed    = false; // false hasta recibir la primera muestra
uint32_t settleRemaining  = 0;     // muestras restantes de estabilización
uint32_t autoResets       = 0;     // reinicios por cambio brusco (total)
uint32_t fingerResets     = 0;     // ...por colocar/retirar el dedo
uint32_t artifactResets   = 0;     // ...por movimiento con el dedo puesto
uint32_t manualResets     = 0;
bool     resetThisSample  = false; // true si la muestra actual provocó un reinicio
bool     resetPendingClass = false; // reinicio a clasificar al terminar la estabilización
float    dcIrBeforeReset  = 0.0f;  // DC IR justo antes del reinicio

// ---- Ventana de 1 segundo ----
uint32_t winStartMs       = 0;
uint32_t winCount         = 0;
uint32_t winAcCount       = 0;     // muestras ya estabilizadas
double   winDcSum[2];
float    winAcMin[2];
float    winAcMax[2];
double   winAcSumSq[2];
double   winAcSum[2];
double   winAcSumProd     = 0.0;   // suma de AC_RED * AC_IR (correlación)
uint32_t winSaturated     = 0;
uint32_t winZeros         = 0;
int      winMaxFifo       = 0;
uint32_t winLostStart     = 0;
uint32_t winI2cErrStart   = 0;
uint32_t winResetsStart   = 0;

// ---- Prueba guiada ----
const int TEST_IDLE             = 0;
const int TEST_BASELINE_WAIT    = 1;
const int TEST_BASELINE_MEASURE = 2;
const int TEST_FINGER_WAIT      = 3;
const int TEST_FINGER_MEASURE   = 4;
const int TEST_REMOVE_WAIT      = 5;
const int TEST_REMOVE_MEASURE   = 6;

const uint32_t TEST_WAIT_SHORT_MS    = 3000;
const uint32_t TEST_WAIT_FINGER_MS   = 5000;
const uint32_t TEST_MEASURE_MS       = 5000;
const uint32_t TEST_FINGER_MEASURE_MS = 10000;
const int      SUBWINDOW_SAMPLES     = 200;   // ventanas de ~2 s en el bloque con dedo
const int      MAX_SUBWINDOWS        = 12;
const int      MIN_SUBWINDOWS_CV     = 3;     // ventanas mínimas para evaluar el CV

int      testPhase         = TEST_IDLE;
uint32_t testPhaseStartMs  = 0;
uint32_t testStartMs       = 0;
uint32_t testStartSamples  = 0;
uint32_t testStartLost     = 0;
uint32_t testStartFingerResets   = 0;
uint32_t testStartArtifactResets = 0;
char     modeBeforeTest    = 'd';

// Bloques: [0] sin dedo, [1] con dedo quieto, [2] retirado. Canal: [0] RED, [1] IR
double   tDcSum[3][2];
double   tAcSumSq[3][2];
double   tAcSum[3][2];
double   tAcSumProd[3];            // suma de AC_RED * AC_IR por bloque
double   tRawSum[3][2];            // suma de valores CRUDOS por bloque
uint32_t tCount[3];
uint32_t tAcCount[3];
uint32_t tSaturated[3];
uint32_t tResets[3];               // reinicios de filtro dentro de cada bloque
int      subDiscarded      = 0;    // ventanas descartadas por reinicio

// Sub-ventanas de ~1 s dentro del bloque con dedo
float    subMin[2], subMax[2];
int      subCount          = 0;
float    subPP[MAX_SUBWINDOWS][2];
int      subWindows        = 0;

// =================================================================
//  MÓDULO: ACCESO I2C DE BAJO NIVEL  (sin cambios, Etapa 3)
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
//  MÓDULO: SENSOR MAX30102  (sin cambios, Etapa 3)
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
//  MÓDULO: PROCESAMIENTO PPG  (nuevo en la Etapa 4)
// =================================================================
// Calcula los coeficientes de los filtros a partir de fs y de las
// frecuencias de corte (transformación bilineal, "Audio EQ Cookbook").
// Filtros Butterworth de 2.º orden: Q = 1/sqrt(2).
void computeFilterCoefficients() {
  // Estimador DC: y[n] = y[n-1] + alpha * (x[n] - y[n-1])
  dcAlpha     = 1.0f - expf(-1.0f / (FS_NOMINAL * DC_TIME_CONSTANT_S));
  dcAlphaFast = 1.0f - expf(-1.0f / (FS_NOMINAL * SETTLE_DC_TIME_CONSTANT_S));

  // Pasa-altas 0.5 Hz
  float w0 = 2.0f * PI * HP_CUTOFF_HZ / FS_NOMINAL;
  float cw = cosf(w0);
  float al = sinf(w0) / (2.0f * BUTTERWORTH_Q);
  float a0 = 1.0f + al;
  hpB0 =  (1.0f + cw) / 2.0f / a0;
  hpB1 = -(1.0f + cw) / a0;
  hpB2 =  (1.0f + cw) / 2.0f / a0;
  hpA1 = (-2.0f * cw) / a0;
  hpA2 = (1.0f - al) / a0;

  // Pasa-bajas 5 Hz
  w0 = 2.0f * PI * LP_CUTOFF_HZ / FS_NOMINAL;
  cw = cosf(w0);
  al = sinf(w0) / (2.0f * BUTTERWORTH_Q);
  a0 = 1.0f + al;
  lpB0 = (1.0f - cw) / 2.0f / a0;
  lpB1 = (1.0f - cw) / a0;
  lpB2 = (1.0f - cw) / 2.0f / a0;
  lpA1 = (-2.0f * cw) / a0;
  lpA2 = (1.0f - al) / a0;
}

void printFilterConfiguration() {
  Serial.println("  Procesamiento PPG (por canal RED e IR):");
  Serial.printf ("    Estimador DC  : paso bajo exponencial, tau = %.1f s, alpha = %.6f\n",
                 DC_TIME_CONSTANT_S, dcAlpha);
  Serial.printf ("    Pasa-altas    : Butterworth 2.o orden, fc = %.2f Hz\n", HP_CUTOFF_HZ);
  Serial.printf ("      b = [%.6f, %.6f, %.6f]  a = [1, %.6f, %.6f]\n",
                 hpB0, hpB1, hpB2, hpA1, hpA2);
  Serial.printf ("    Pasa-bajas    : Butterworth 2.o orden, fc = %.2f Hz\n", LP_CUTOFF_HZ);
  Serial.printf ("      b = [%.6f, %.6f, %.6f]  a = [1, %.6f, %.6f]\n",
                 lpB0, lpB1, lpB2, lpA1, lpA2);
  Serial.printf ("    Reinicio automatico si |x - DC| > max(%.0f %% del DC, %.0f cuentas)\n",
                 STEP_RELATIVE * 100.0f, STEP_MIN_COUNTS);
  Serial.printf ("    Estabilizacion tras reinicio: %lu muestras (%.1f s), DC rapido tau = %.1f s,\n",
                 (unsigned long)SETTLE_SAMPLES, SETTLE_SAMPLES / FS_NOMINAL, SETTLE_DC_TIME_CONSTANT_S);
  Serial.println("      sin nuevos reinicios durante la estabilizacion (periodo refractario)");
  Serial.println("    PPG mostrada = -AC (el pico sistolico queda hacia arriba)\n");
}

// Biquad pasa-altas, Forma Directa II Transpuesta, canal ch
float highPass(int ch, float x) {
  float y  = hpB0 * x + hpZ1[ch];
  hpZ1[ch] = hpB1 * x - hpA1 * y + hpZ2[ch];
  hpZ2[ch] = hpB2 * x - hpA2 * y;
  return y;
}

// Biquad pasa-bajas, Forma Directa II Transpuesta, canal ch
float lowPass(int ch, float x) {
  float y  = lpB0 * x + lpZ1[ch];
  lpZ1[ch] = lpB1 * x - lpA1 * y + lpZ2[ch];
  lpZ2[ch] = lpB2 * x - lpA2 * y;
  return y;
}

// Reinicia ambos canales con los valores actuales como DC inicial.
// Se reinician juntos para que RED e IR permanezcan alineados.
void resetFilters(uint32_t red, uint32_t ir) {
  dcEstimate[CH_RED] = (float)red;
  dcEstimate[CH_IR]  = (float)ir;
  for (int ch = 0; ch < 2; ch++) {
    hpZ1[ch] = 0.0f;  hpZ2[ch] = 0.0f;
    lpZ1[ch] = 0.0f;  lpZ2[ch] = 0.0f;
    lastDc[ch] = dcEstimate[ch];
    lastAc[ch] = 0.0f;
  }
  settleRemaining = SETTLE_SAMPLES;
  filtersPrimed = true;
}

// Cambio brusco (dedo colocado/retirado, movimiento fuerte)
bool isLargeStep(int ch, float x) {
  float dc = dcEstimate[ch];
  float limit = STEP_RELATIVE * dc;
  if (limit < STEP_MIN_COUNTS) limit = STEP_MIN_COUNTS;
  return fabsf(x - dc) > limit;
}

// Registra un reinicio automático. Se clasifica al TERMINAR la
// estabilización (classifyPendingReset), cuando el DC ya alcanzó su
// nuevo nivel; al inicio de una rampa todavía no se sabe a dónde llegará.
void registerAutoReset() {
  autoResets++;
  dcIrBeforeReset   = dcEstimate[CH_IR];
  resetPendingClass = true;
}

// Clasificación:
//  - transición de dedo: el DC IR cruzó el umbral de presencia
//    (dedo colocado o retirado);
//  - artefacto: el dedo siguió en la misma condición (movimiento o
//    cambio fuerte de presión).
void classifyPendingReset() {
  if (!resetPendingClass) return;
  bool fingerBefore = (dcIrBeforeReset >= (float)FINGER_IR_THRESHOLD);
  bool fingerAfter  = (dcEstimate[CH_IR] >= (float)FINGER_IR_THRESHOLD);
  if (fingerBefore != fingerAfter) fingerResets++;
  else artifactResets++;
  resetPendingClass = false;
}

// Procesa una muestra de ambos canales. Resultados en lastDc[] y lastAc[].
//
// Manejo de cambios bruscos (corregido en la Etapa 4.1):
//  1. Fuera de la estabilización, un cambio > max(10 % DC, 5000) reinicia
//     los filtros con el valor actual (DC = x, biquads en cero).
//  2. Durante los 2 s de estabilización NO se permiten nuevos reinicios
//     (periodo refractario). Así una rampa de colocación o retirada del
//     dedo produce UN reinicio, no decenas.
//  3. Primer segundo de la estabilización: solo el DC sigue a la señal
//     con tau = 0.1 s (absorbe el resto de la rampa); los filtros
//     pasa-banda permanecen detenidos y en cero, para que la rampa no
//     genere un transitorio grande en el pasa-altas.
//  4. Segundo segundo: procesamiento normal (tau = 1 s) para que los
//     filtros se "calienten". El DC es continuo en ambos cambios.
//  5. Las muestras en estabilización no se usan en las estadísticas AC.
void processPPG(uint32_t red, uint32_t ir) {
  resetThisSample = false;
  if (!filtersPrimed) {
    resetFilters(red, ir);
    resetThisSample = true;
    return;
  }

  bool settling = (settleRemaining > 0);
  if (!settling && (isLargeStep(CH_RED, (float)red) || isLargeStep(CH_IR, (float)ir))) {
    registerAutoReset();
    resetFilters(red, ir);
    resetThisSample = true;
    return;
  }

  float x[2];
  x[CH_RED] = (float)red;
  x[CH_IR]  = (float)ir;
  bool fastDcPhase = (settleRemaining > SETTLE_SAMPLES - SETTLE_FAST_DC_SAMPLES);

  for (int ch = 0; ch < 2; ch++) {
    if (fastDcPhase) {
      // Fase 1: DC rápido, filtros detenidos (AC = 0)
      dcEstimate[ch] += dcAlphaFast * (x[ch] - dcEstimate[ch]);
      lastDc[ch] = dcEstimate[ch];
      lastAc[ch] = 0.0f;
    } else {
      // Fase 2 y operación normal
      float centered = x[ch] - dcEstimate[ch];              // señal sin DC
      dcEstimate[ch] += dcAlpha * (x[ch] - dcEstimate[ch]); // actualiza DC
      float ac = lowPass(ch, highPass(ch, centered));       // pasa-banda 0.5-5 Hz
      lastDc[ch] = dcEstimate[ch];
      lastAc[ch] = ac;
    }
  }
  if (settleRemaining > 0) {
    settleRemaining--;
    if (settleRemaining == 0) classifyPendingReset();
  }
}

bool filtersSettled() {
  return filtersPrimed && settleRemaining == 0;
}

// Coeficiente de correlación de Pearson entre AC_RED y AC_IR a partir
// de sumas acumuladas. El pulso arterial aparece simultáneamente en
// ambos canales (correlación alta); el ruido electrónico de cada canal
// es independiente (correlación baja). Es un indicador de calidad de
// señal, NO una detección de latidos.
float correlationFromSums(double n, double sumR, double sumI,
                          double sumRR, double sumII, double sumRI) {
  if (n < 2.0) return 0.0f;
  double cov  = sumRI - sumR * sumI / n;
  double varR = sumRR - sumR * sumR / n;
  double varI = sumII - sumI * sumI / n;
  if (varR <= 0.0 || varI <= 0.0) return 0.0f;
  return (float)(cov / sqrt(varR * varI));
}

// =================================================================
//  MÓDULO: ESTADÍSTICAS POR SEGUNDO
// =================================================================
void resetWindow(uint32_t nowMs) {
  winStartMs     = nowMs;
  winCount       = 0;
  winAcCount     = 0;
  for (int ch = 0; ch < 2; ch++) {
    winDcSum[ch]   = 0.0;
    winAcMin[ch]   = 1e9f;
    winAcMax[ch]   = -1e9f;
    winAcSumSq[ch] = 0.0;
    winAcSum[ch]   = 0.0;
  }
  winAcSumProd   = 0.0;
  winSaturated   = 0;
  winZeros       = 0;
  winMaxFifo     = 0;
  winLostStart   = lostSamples;
  winI2cErrStart = i2cErrors;
  winResetsStart = autoResets;
}

void accumulateWindow(uint32_t red, uint32_t ir, int fifoLevel) {
  winCount++;
  if (red >= SATURATION_LEVEL || ir >= SATURATION_LEVEL) winSaturated++;
  if (red == 0 || ir == 0) winZeros++;
  if (fifoLevel > winMaxFifo) winMaxFifo = fifoLevel;
  for (int ch = 0; ch < 2; ch++) winDcSum[ch] += lastDc[ch];

  if (filtersSettled()) {
    winAcCount++;
    for (int ch = 0; ch < 2; ch++) {
      float ac = lastAc[ch];
      if (ac < winAcMin[ch]) winAcMin[ch] = ac;
      if (ac > winAcMax[ch]) winAcMax[ch] = ac;
      winAcSumSq[ch] += (double)ac * ac;
      winAcSum[ch]   += ac;
    }
    winAcSumProd += (double)lastAc[CH_RED] * lastAc[CH_IR];
  }
}

void printChannelLine(const char *name, int ch) {
  float dc = (winCount > 0) ? (float)(winDcSum[ch] / winCount) : 0.0f;
  if (winAcCount > 0) {
    float pp  = winAcMax[ch] - winAcMin[ch];
    float rms = sqrtf((float)(winAcSumSq[ch] / winAcCount));
    float acdc = (dc > 0.0f) ? (pp / dc * 100.0f) : 0.0f;
    Serial.printf("   %s: DC %8.0f | AC p-p %8.1f | AC rms %7.1f | AC/DC %6.3f %%\n",
                  name, dc, pp, rms, acdc);
  } else {
    Serial.printf("   %s: DC %8.0f | AC: estabilizando filtros...\n", name, dc);
  }
}

void reportWindow(uint32_t nowMs) {
  uint32_t elapsed = nowMs - winStartMs;
  if (elapsed < 1000) return;

  if (outputMode == 'd') {
    float rate = winCount * 1000.0f / elapsed;
    uint32_t lostNow   = lostSamples - winLostStart;
    uint32_t errNow    = i2cErrors - winI2cErrStart;
    uint32_t resetsNow = autoResets - winResetsStart;
    float meanIrDc = (winCount > 0) ? (float)(winDcSum[CH_IR] / winCount) : 0.0f;
    bool finger = (meanIrDc >= FINGER_IR_THRESHOLD);

    Serial.printf("== t=%lus | %.1f muestras/s | FIFO max %d | %s | alertas:",
                  (unsigned long)(nowMs / 1000UL), rate, winMaxFifo,
                  finger ? "DEDO" : "SIN DEDO");
    bool anyAlert = false;
    if (winCount == 0)    { Serial.print(" SIN_MUESTRAS"); anyAlert = true; }
    if (winSaturated > 0) { Serial.printf(" SATURACION(%lu)", (unsigned long)winSaturated); anyAlert = true; }
    if (winZeros > 0)     { Serial.printf(" CEROS(%lu)", (unsigned long)winZeros); anyAlert = true; }
    if (lostNow > 0)      { Serial.printf(" PERDIDAS(%lu)", (unsigned long)lostNow); anyAlert = true; }
    if (errNow > 0)       { Serial.printf(" ERR_I2C(%lu)", (unsigned long)errNow); anyAlert = true; }
    if (resetsNow > 0)    { Serial.printf(" REINICIO_FILTROS(%lu)", (unsigned long)resetsNow); anyAlert = true; }
    if (winCount > 0 && (rate < RATE_MIN || rate > RATE_MAX)) { Serial.print(" TASA"); anyAlert = true; }
    if (!anyAlert) Serial.print(" ninguna");
    Serial.println();
    printChannelLine("RED", CH_RED);
    printChannelLine("IR ", CH_IR);
    if (winAcCount > 1) {
      float corr = correlationFromSums((double)winAcCount, winAcSum[CH_RED], winAcSum[CH_IR],
                                       winAcSumSq[CH_RED], winAcSumSq[CH_IR], winAcSumProd);
      Serial.printf("   Correlacion AC RED-IR: %.2f\n", corr);
    }
  }
  resetWindow(nowMs);
}

// =================================================================
//  MÓDULO: PRUEBA GUIADA DE LA SEÑAL PPG
// =================================================================
void resetTestAccumulators() {
  for (int k = 0; k < 3; k++) {
    for (int ch = 0; ch < 2; ch++) {
      tDcSum[k][ch] = 0.0;
      tAcSumSq[k][ch] = 0.0;
      tAcSum[k][ch] = 0.0;
      tRawSum[k][ch] = 0.0;
    }
    tAcSumProd[k] = 0.0;
    tCount[k] = 0;
    tAcCount[k] = 0;
    tSaturated[k] = 0;
    tResets[k] = 0;
  }
  for (int ch = 0; ch < 2; ch++) {
    subMin[ch] = 1e9f;
    subMax[ch] = -1e9f;
  }
  subCount = 0;
  subWindows = 0;
  subDiscarded = 0;
}

// Descarta la ventana de ~2 s en curso (contiene un reinicio de filtro)
void discardSubwindow() {
  if (subCount > 0) subDiscarded++;
  for (int ch = 0; ch < 2; ch++) {
    subMin[ch] = 1e9f;
    subMax[ch] = -1e9f;
  }
  subCount = 0;
}

int activeTestBlock() {
  if (testPhase == TEST_BASELINE_MEASURE) return 0;
  if (testPhase == TEST_FINGER_MEASURE)   return 1;
  if (testPhase == TEST_REMOVE_MEASURE)   return 2;
  return -1;
}

void accumulateTest(uint32_t red, uint32_t ir) {
  int k = activeTestBlock();
  if (k < 0) return;

  tCount[k]++;
  if (red >= SATURATION_LEVEL || ir >= SATURATION_LEVEL) tSaturated[k]++;
  for (int ch = 0; ch < 2; ch++) tDcSum[k][ch] += lastDc[ch];
  tRawSum[k][CH_RED] += red;
  tRawSum[k][CH_IR]  += ir;

  // Un reinicio dentro del bloque se cuenta y, en el bloque con dedo,
  // invalida la ventana de 2 s en curso (no se mezcla con señal PPG).
  if (resetThisSample) {
    tResets[k]++;
    if (k == 1) discardSubwindow();
  }

  if (!filtersSettled()) return;
  tAcCount[k]++;
  for (int ch = 0; ch < 2; ch++) {
    tAcSumSq[k][ch] += (double)lastAc[ch] * lastAc[ch];
    tAcSum[k][ch]   += lastAc[ch];
  }
  tAcSumProd[k] += (double)lastAc[CH_RED] * lastAc[CH_IR];

  // En el bloque con dedo se mide el p-p en ventanas de ~2 s
  // (2 s contienen al menos un ciclo completo para pulsos >= 30 lpm)
  if (k == 1) {
    for (int ch = 0; ch < 2; ch++) {
      if (lastAc[ch] < subMin[ch]) subMin[ch] = lastAc[ch];
      if (lastAc[ch] > subMax[ch]) subMax[ch] = lastAc[ch];
    }
    subCount++;
    if (subCount >= SUBWINDOW_SAMPLES) {
      if (subWindows < MAX_SUBWINDOWS) {
        subPP[subWindows][CH_RED] = subMax[CH_RED] - subMin[CH_RED];
        subPP[subWindows][CH_IR]  = subMax[CH_IR]  - subMin[CH_IR];
        subWindows++;
      }
      for (int ch = 0; ch < 2; ch++) {
        subMin[ch] = 1e9f;
        subMax[ch] = -1e9f;
      }
      subCount = 0;
    }
  }
}

float testMeanDc(int k, int ch) {
  return (tCount[k] > 0) ? (float)(tDcSum[k][ch] / tCount[k]) : 0.0f;
}

float testMeanRaw(int k, int ch) {
  return (tCount[k] > 0) ? (float)(tRawSum[k][ch] / tCount[k]) : 0.0f;
}

float testAcRms(int k, int ch) {
  return (tAcCount[k] > 0) ? sqrtf((float)(tAcSumSq[k][ch] / tAcCount[k])) : 0.0f;
}

float subMeanPP(int ch) {
  if (subWindows == 0) return 0.0f;
  float s = 0.0f;
  for (int i = 0; i < subWindows; i++) s += subPP[i][ch];
  return s / subWindows;
}

float subMinPP(int ch) {
  if (subWindows == 0) return 0.0f;
  float m = subPP[0][ch];
  for (int i = 1; i < subWindows; i++) if (subPP[i][ch] < m) m = subPP[i][ch];
  return m;
}

// Coeficiente de variación (desviación estándar / media) del p-p
float subCvPP(int ch) {
  if (subWindows < 2) return 0.0f;
  float mean = subMeanPP(ch);
  if (mean <= 0.0f) return 99.0f;
  float s = 0.0f;
  for (int i = 0; i < subWindows; i++) {
    float d = subPP[i][ch] - mean;
    s += d * d;
  }
  return sqrtf(s / (subWindows - 1)) / mean;
}

void printTestResult(const char *name, bool ok) {
  Serial.printf("  [%s] %s\n", ok ? " OK " : "FALLA", name);
}

void startPpgTest(uint32_t nowMs) {
  if (!sensorReady) {
    Serial.println(">> El sensor no esta listo; no se puede iniciar la prueba.");
    return;
  }
  resetTestAccumulators();
  modeBeforeTest   = outputMode;
  outputMode       = 'd';
  testStartMs      = nowMs;
  testStartSamples = sampleIndex;
  testStartLost    = lostSamples;
  testStartFingerResets   = fingerResets;
  testStartArtifactResets = artifactResets;
  testPhase        = TEST_BASELINE_WAIT;
  testPhaseStartMs = nowMs;

  Serial.println("\n================================================");
  Serial.println(" PRUEBA GUIADA DE LA SENAL PPG (aprox. 31 s)");
  Serial.println(" Salida en modo diagnostico durante la prueba.");
  Serial.println("================================================");
  Serial.println(">> FASE 1: RETIRE el dedo del sensor y no lo toque.");
  Serial.println("   (3 s de espera + 5 s de medicion del ruido de fondo)");
}

void finishPpgTest(uint32_t nowMs) {
  float dcIr0 = testMeanDc(0, CH_IR), dcIr1 = testMeanDc(1, CH_IR), dcIr2 = testMeanDc(2, CH_IR);
  float dcRed0 = testMeanDc(0, CH_RED), dcRed1 = testMeanDc(1, CH_RED), dcRed2 = testMeanDc(2, CH_RED);
  float rmsIr0 = testAcRms(0, CH_IR), rmsIr1 = testAcRms(1, CH_IR), rmsIr2 = testAcRms(2, CH_IR);
  float rmsRed0 = testAcRms(0, CH_RED), rmsRed1 = testAcRms(1, CH_RED), rmsRed2 = testAcRms(2, CH_RED);

  float ppIr     = subMeanPP(CH_IR);
  float ppRed    = subMeanPP(CH_RED);
  float ppIrMin  = subMinPP(CH_IR);
  float cvIr     = subCvPP(CH_IR);
  float acdcIr   = (dcIr1 > 0.0f) ? ppIr / dcIr1 * 100.0f : 0.0f;
  float acdcRed  = (dcRed1 > 0.0f) ? ppRed / dcRed1 * 100.0f : 0.0f;
  float noiseRef = (rmsIr0 > 1.0f) ? rmsIr0 : 1.0f;      // evita dividir entre ~0
  float acRatio  = rmsIr1 / noiseRef;
  float dDcIr    = dcIr1 - dcIr0;
  // Recuperación = (DC1 - DC2) / (DC1 - DC0): 1.0 = volvió al fondo, 0 = no bajó
  float recovery = (dDcIr > 0.0f) ? (dcIr1 - dcIr2) / dDcIr : 0.0f;
  // La misma fórmula con los valores CRUDOS permite distinguir si el que no
  // bajó fue el DC (error del algoritmo) o la señal del sensor (dedo o mano
  // todavía sobre el sensor, reflejando luz IR).
  float rawIr0 = testMeanRaw(0, CH_IR), rawIr1 = testMeanRaw(1, CH_IR), rawIr2 = testMeanRaw(2, CH_IR);
  float dRawIr = rawIr1 - rawIr0;
  float rawRecovery = (dRawIr > 0.0f) ? (rawIr1 - rawIr2) / dRawIr : 0.0f;
  uint32_t testFingerResets   = fingerResets - testStartFingerResets;
  uint32_t testArtifactResets = artifactResets - testStartArtifactResets;
  float corr1   = correlationFromSums((double)tAcCount[1], tAcSum[1][CH_RED], tAcSum[1][CH_IR],
                                       tAcSumSq[1][CH_RED], tAcSumSq[1][CH_IR], tAcSumProd[1]);

  uint32_t testSamples = sampleIndex - testStartSamples;
  uint32_t testLost    = lostSamples - testStartLost;
  float    testRate    = testSamples * 1000.0f / (float)(nowMs - testStartMs);
  uint32_t satTotal    = tSaturated[0] + tSaturated[1] + tSaturated[2];

  bool ok1  = (dDcIr >= MIN_DC_DELTA_IR);
  bool ok2  = (acRatio >= MIN_AC_RATIO);
  bool ok3  = (ppIr >= MIN_PP_IR) && (acdcIr >= MIN_ACDC_PERCENT);
  bool ok4  = (ppRed >= MIN_PP_RED);
  bool ok5  = (subWindows > 0) && (ppIrMin >= FLAT_PP_LIMIT);
  bool ok6  = (subWindows >= MIN_SUBWINDOWS_CV) && (cvIr <= MAX_PP_CV);
  bool ok7  = (recovery >= MIN_RECOVERY);
  bool ok8  = (satTotal == 0);
  bool ok9  = (testLost == 0);
  bool ok10 = (testRate >= RATE_MIN && testRate <= RATE_MAX);
  bool ok11 = (corr1 >= MIN_RED_IR_CORR);

  Serial.println("\n================================================");
  Serial.println(" RESULTADOS DE LA PRUEBA PPG (cuentas del ADC, no valores clinicos)");
  Serial.println("================================================");
  Serial.println("  Bloque          IR crudo     DC RED     DC IR   AC rms RED  AC rms IR  reinicios");
  Serial.printf ("  1. Sin dedo    %9.0f  %9.0f %9.0f   %9.1f  %9.1f  %5lu\n",
                 rawIr0, dcRed0, dcIr0, rmsRed0, rmsIr0, (unsigned long)tResets[0]);
  Serial.printf ("  2. Dedo quieto %9.0f  %9.0f %9.0f   %9.1f  %9.1f  %5lu\n",
                 rawIr1, dcRed1, dcIr1, rmsRed1, rmsIr1, (unsigned long)tResets[1]);
  Serial.printf ("  3. Retirado    %9.0f  %9.0f %9.0f   %9.1f  %9.1f  %5lu\n",
                 rawIr2, dcRed2, dcIr2, rmsRed2, rmsIr2, (unsigned long)tResets[2]);
  Serial.println("------------------------------------------------");
  Serial.printf ("  Ventanas de 2 s con dedo: %d validas, %d descartadas por reinicio\n",
                 subWindows, subDiscarded);
  Serial.print  ("  AC p-p IR por ventana :");
  for (int i = 0; i < subWindows; i++) Serial.printf(" %.0f", subPP[i][CH_IR]);
  Serial.println();
  Serial.print  ("  AC p-p RED por ventana:");
  for (int i = 0; i < subWindows; i++) Serial.printf(" %.0f", subPP[i][CH_RED]);
  Serial.println();
  Serial.printf ("  AC p-p medio: IR %.1f | RED %.1f   AC/DC: IR %.3f %% | RED %.3f %%\n",
                 ppIr, ppRed, acdcIr, acdcRed);
  Serial.printf ("  AC rms IR con dedo / sin dedo: %.1f veces\n", acRatio);
  Serial.printf ("  Variacion de amplitud IR (CV): %.0f %%\n", cvIr * 100.0f);
  Serial.printf ("  Correlacion AC RED-IR con dedo: %.2f\n", corr1);
  Serial.printf ("  Recuperacion al retirar: DC IR %.0f %% | IR crudo %.0f %%\n",
                 recovery * 100.0f, rawRecovery * 100.0f);
  Serial.printf ("  Tasa: %.1f muestras/s | perdidas: %lu | saturadas: %lu\n",
                 testRate, (unsigned long)testLost, (unsigned long)satTotal);
  Serial.printf ("  Reinicios de filtro DURANTE la prueba: %lu por dedo colocado/retirado, %lu por artefacto\n",
                 (unsigned long)testFingerResets, (unsigned long)testArtifactResets);
  Serial.println("------------------------------------------------");
  printTestResult("1. La senal cambia al colocar el dedo (DC IR +10000)", ok1);
  printTestResult("2. Aparece componente AC (rms IR >= 3x el fondo)", ok2);
  printTestResult("3. Componente pulsatil IR observable (p-p >= 100, AC/DC >= 0.05 %)", ok3);
  printTestResult("4. Componente pulsatil RED observable (p-p >= 50)", ok4);
  printTestResult("5. La senal no queda plana (cada ventana p-p >= 20)", ok5);
  printTestResult("6. Amplitud estable con dedo quieto (CV <= 50 %, >= 3 ventanas de 2 s)", ok6);
  printTestResult("7. El DC se recupera al retirar el dedo (>= 80 %)", ok7);
  printTestResult("8. Sin saturacion del ADC", ok8);
  printTestResult("9. Sin muestras perdidas", ok9);
  printTestResult("10. Tasa de muestreo entre 95 y 105 muestras/s", ok10);
  printTestResult("11. RED e IR pulsan juntas (correlacion AC >= 0.70)", ok11);
  Serial.println("------------------------------------------------");

  if (ok1 && ok2 && ok3 && ok4 && ok5 && ok6 && ok7 && ok8 && ok9 && ok10 && ok11) {
    Serial.println(" RESULTADO: PRUEBA DE PROCESAMIENTO PPG SUPERADA");
    Serial.println(" Se observa una componente pulsatil filtrada en RED e IR.");
    Serial.println(" (Esta etapa NO calcula ni valida frecuencia cardiaca.)");
  } else {
    Serial.println(" RESULTADO: PRUEBA DE PROCESAMIENTO PPG NO SUPERADA. Sugerencias:");
    if (!ok1) Serial.println("  - Cubra completamente la ventana del sensor con la yema del dedo.");
    if (!ok2 || !ok3 || !ok4 || !ok5 || !ok11) {
      Serial.println("  - Pulsatilidad baja: apoye el dedo sin presionar (la presion aplasta");
      Serial.println("    los capilares), caliente las manos o aumente la corriente con '+'.");
    }
    if (!ok6) Serial.println("  - Amplitud variable o pocas ventanas validas: mantenga dedo y cable quietos.");
    if (!ok7) {
      if (rawRecovery < MIN_RECOVERY) {
        Serial.println("  - El IR CRUDO tampoco bajo: el sensor seguia recibiendo luz reflejada.");
        Serial.println("    Aleje por completo el dedo y la mano (el IR se refleja a varios cm).");
      } else {
        Serial.println("  - El crudo bajo pero el DC no: posible falla del estimador DC.");
      }
    }
    if (!ok8) Serial.println("  - Saturacion: reduzca la corriente con '-'.");
    if (!ok9 || !ok10) Serial.println("  - Problema de adquisicion: revise alertas en el diagnostico.");
  }
  Serial.println(" (Prueba experimental. No es una medicion clinica.)");
  Serial.println("================================================\n");

  testPhase  = TEST_IDLE;
  outputMode = modeBeforeTest;
}

void updatePpgTest(uint32_t nowMs) {
  if (testPhase == TEST_IDLE) return;
  uint32_t elapsed = nowMs - testPhaseStartMs;

  if (testPhase == TEST_BASELINE_WAIT && elapsed >= TEST_WAIT_SHORT_MS) {
    testPhase = TEST_BASELINE_MEASURE;
    testPhaseStartMs = nowMs;
    Serial.println(">> Midiendo fondo SIN dedo (5 s)...");
  } else if (testPhase == TEST_BASELINE_MEASURE && elapsed >= TEST_MEASURE_MS) {
    testPhase = TEST_FINGER_WAIT;
    testPhaseStartMs = nowMs;
    Serial.println(">> FASE 2: COLOQUE la yema del dedo sobre el sensor, SIN presionar.");
    Serial.println("   Apoye la mano sobre la mesa. (5 s de estabilizacion)");
  } else if (testPhase == TEST_FINGER_WAIT && elapsed >= TEST_WAIT_FINGER_MS) {
    testPhase = TEST_FINGER_MEASURE;
    testPhaseStartMs = nowMs;
    Serial.println(">> FASE 3: MANTENGA el dedo QUIETO. Midiendo 10 s...");
  } else if (testPhase == TEST_FINGER_MEASURE && elapsed >= TEST_FINGER_MEASURE_MS) {
    testPhase = TEST_REMOVE_WAIT;
    testPhaseStartMs = nowMs;
    Serial.println(">> FASE 4: RETIRE el dedo del sensor.");
    Serial.println("   (3 s de espera + 5 s de medicion)");
  } else if (testPhase == TEST_REMOVE_WAIT && elapsed >= TEST_WAIT_SHORT_MS) {
    testPhase = TEST_REMOVE_MEASURE;
    testPhaseStartMs = nowMs;
    Serial.println(">> Midiendo SIN dedo nuevamente (5 s)...");
  } else if (testPhase == TEST_REMOVE_MEASURE && elapsed >= TEST_MEASURE_MS) {
    finishPpgTest(nowMs);
  }
}

// =================================================================
//  MÓDULO: TRATAMIENTO DE CADA MUESTRA
// =================================================================
void processSample(uint32_t red, uint32_t ir, int fifoLevel) {
  sampleIndex++;
  processPPG(red, ir);                 // DC, AC y PPG de ambos canales
  accumulateWindow(red, ir, fifoLevel);
  accumulateTest(red, ir);

  if (outputMode == 'p') {
    if (sampleIndex % DECIMATION == 0) {
      Serial.printf("#%lu RED=%6lu IR=%6lu | DC R=%8.1f I=%8.1f | AC R=%8.1f I=%8.1f%s\n",
                    (unsigned long)sampleIndex, (unsigned long)red, (unsigned long)ir,
                    lastDc[CH_RED], lastDc[CH_IR], lastAc[CH_RED], lastAc[CH_IR],
                    filtersSettled() ? "" : "  (estabilizando)");
    }
  } else if (outputMode == 'g') {
    // PPG invertida para el Serial Plotter (pico sistólico hacia arriba)
    Serial.printf("IR_PPG:%.1f,RED_PPG:%.1f\n", -lastAc[CH_IR], -lastAc[CH_RED]);
  } else if (outputMode == 'x') {
    Serial.printf("%lu,%lu,%lu,%.1f,%.1f,%.2f,%.2f\n",
                  (unsigned long)sampleIndex, (unsigned long)red, (unsigned long)ir,
                  lastDc[CH_RED], lastDc[CH_IR], lastAc[CH_RED], lastAc[CH_IR]);
  }
}

// =================================================================
//  MÓDULO: LECTURA DEL FIFO DEL MAX30102  (sin cambios, Etapa 3)
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
  Serial.println("  d -> diagnostico por segundo (DC, AC p-p, AC rms, AC/DC)");
  Serial.println("  p -> procesamiento: crudo, DC y AC de RED e IR (1 de cada 10)");
  Serial.println("  g -> Serial Plotter: PPG de IR y RED (todas las muestras)");
  Serial.println("  x -> CSV: n,RED,IR,RED_DC,IR_DC,RED_AC,IR_AC");
  Serial.println("  t -> prueba guiada de la senal PPG (aprox. 31 s)");
  Serial.println("  r -> reiniciar filtros");
  Serial.println("  + -> aumentar corriente de LED (1 mA)");
  Serial.println("  - -> disminuir corriente de LED (1 mA)");
  Serial.println("  i -> configuracion, registros y coeficientes");
  Serial.println("  h -> ayuda\n");
}

void setOutputMode(char mode) {
  if (testPhase != TEST_IDLE) {
    Serial.println(">> Prueba en curso: el modo se aplicara al terminar.");
    modeBeforeTest = mode;
    return;
  }
  outputMode = mode;
  if (mode == 'd') Serial.println(">> Modo: diagnostico por segundo");
  if (mode == 'p') Serial.println(">> Modo: procesamiento (1 de cada 10 muestras)");
  if (mode == 'x') Serial.println("n,RED,IR,RED_DC,IR_DC,RED_AC,IR_AC");
  // En modo 'g' no se imprime texto extra para no alterar el Serial Plotter
}

void handleSerialCommands(uint32_t nowMs) {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r' || c == '\n' || c == ' ') continue;

    if (c == 'd' || c == 'D') {
      setOutputMode('d');
    } else if (c == 'p' || c == 'P') {
      setOutputMode('p');
    } else if (c == 'g' || c == 'G') {
      setOutputMode('g');
    } else if (c == 'x' || c == 'X') {
      setOutputMode('x');
    } else if (c == 't' || c == 'T') {
      if (testPhase == TEST_IDLE) startPpgTest(nowMs);
      else Serial.println(">> Ya hay una prueba en curso.");
    } else if (c == 'r' || c == 'R') {
      if (testPhase != TEST_IDLE) {
        Serial.println(">> No se reinician los filtros durante la prueba.");
      } else {
        filtersPrimed = false;       // se reinician con la siguiente muestra
        manualResets++;
        Serial.println(">> Filtros reiniciados (2 s de estabilizacion).");
      }
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
      printFilterConfiguration();
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
  Serial.println(" ETAPA 4.1 - Procesamiento de la senal PPG");
  Serial.println(" Prototipo academico - NO es dispositivo medico");
  Serial.println(" Esta etapa NO calcula frecuencia cardiaca ni SpO2");
  Serial.println("================================================");

  computeFilterCoefficients();
  sensorReady = initSensor();
  printFilterConfiguration();

  if (sensorReady) {
    Serial.println("Adquisicion y procesamiento iniciados. 't' = prueba guiada, 'h' = ayuda.\n");
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
      filtersPrimed = false;
      resetWindow(millis());
    }
    handleSerialCommands(now);
    return;
  }

  // Adquisición: vaciar el FIFO cada POLL_INTERVAL_MS (cada muestra se
  // procesa dentro de processSample, llamada desde readFifo)
  if (now - lastPollMs >= POLL_INTERVAL_MS) {
    lastPollMs = now;
    readFifo();
  }

  // Comandos del usuario
  handleSerialCommands(now);

  // Prueba guiada (máquina de estados no bloqueante)
  updatePpgTest(now);

  // Diagnóstico de cada segundo
  reportWindow(now);
}
