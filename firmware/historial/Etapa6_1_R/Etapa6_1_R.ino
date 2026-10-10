/*
 * =====================================================================
 *  ETAPA 6.1 - Cálculo de R por latido (base para SpO2)
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
 *  El BPM y la razón R son mediciones experimentales, NO clínicas.
 *  Esta etapa calcula R por latido pero TODAVÍA NO la convierte a SpO2.
 *
 *  Conexiones (SIN TFT):
 *    MAX30102 VIN -> 3V3      MAX30102 SDA -> GPIO 21
 *    MAX30102 GND -> GND      MAX30102 SCL -> GPIO 22
 *    INT y RD sin conectar
 *
 *  Adquisición: IDÉNTICA a la Etapa 3 validada (FIFO leído con Wire,
 *  400 kHz, promedio 4, 400 sps internos -> ~100 muestras/s efectivas).
 *
 *  Procesamiento PPG: IDÉNTICO a la Etapa 4.1 validada:
 *    x ──► Estimador DC (paso bajo exponencial, tau = 1 s) ──► DC
 *    x - DC ──► Pasa-altas Butterworth 2.º orden 0.5 Hz
 *           ──► Pasa-bajas Butterworth 2.º orden 5 Hz ──► AC
 *    PPG = -AC  (invertida: el pico sistólico queda hacia arriba)
 *    Estabilización tras reinicio: 1 s de DC rápido (tau = 0.1 s) +
 *    1 s de calentamiento, con periodo refractario de reinicios.
 *
 *  Nuevo en la Etapa 5 (módulo FRECUENCIA CARDIACA), sobre la PPG IR:
 *    - Envolvente adaptativa y umbral = 50 % de la envolvente.
 *    - Máximo local por cambio de signo de la pendiente + prominencia
 *      mínima respecto al valle previo.
 *    - Periodo refractario adaptativo; el pulso se confirma al terminar
 *      el periodo, conservando el máximo más alto.
 *    - Tiempo del pico con interpolación parabólica y frecuencia de
 *      muestreo MEDIDA (no la nominal).
 *    - Rechazo de intervalos fuera de 40-180 BPM o que difieren > 30 %
 *      de la mediana reciente.
 *    - BPM estable = 60000 / media de hasta 8 intervalos aceptados.
 *    - Estados: SIN SENAL / ESTABILIZANDO / BPM VALIDO.
 *
 *  Nuevo en la Etapa 6.1 (módulo CÁLCULO DE R POR LATIDO):
 *    - Búfer de ~2.6 s con AC y DC de RED e IR.
 *    - En cada intervalo ACEPTADO por el detector de BPM (sin modificar
 *      su lógica; una sola llamada en hrCommitPulse):
 *        R = (AC_RED/DC_RED) / (AC_IR/DC_IR). Estimador principal:
 *        pendiente de regresión AC_RED vs AC_IR (insensible al ruido del
 *        canal rojo); comparación: pico a pico y RMS. Además perfusión
 *        (PI), correlación RED-IR y cambio del DC dentro del latido.
 *    - Mediana y CV de los últimos 8 valores de R de regresión
 *      (estimador principal); verificación cruzada |R_pp - R_rms|.
 *
 *  NO se implementa: conversión de R a SpO2, TFT, WiFi, Bluetooth ni
 *  alarmas.
 *
 *  Monitor Serie a 115200 baudios. Comandos:
 *    o -> monitor de R: una línea por latido + resumen por segundo
 *    c -> CSV con una fila por latido (R, AC, DC, PI, correlación)
 *    u -> prueba guiada de R (~56 s)
 *    b -> monitor de BPM: una línea por pulso + resumen por segundo
 *    k -> Serial Plotter: PPG IR, umbral adaptativo y picos detectados
 *    v -> prueba guiada de BPM (~46 s)
 *    d -> diagnóstico PPG por segundo (DC, AC p-p, AC RMS, AC/DC)
 *    p -> procesamiento: crudo, DC y AC de RED e IR (1 de cada 10)
 *    g -> Serial Plotter: PPG de IR y RED (todas las muestras)
 *    x -> CSV: n,RED,IR,RED_DC,IR_DC,RED_AC,IR_AC
 *    t -> prueba guiada de la señal PPG de la Etapa 4.1 (~31 s)
 *    r -> reiniciar filtros (y detector de pulsos)
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
//  CONFIGURACIÓN DE LA FRECUENCIA CARDIACA (nuevo en la Etapa 5)
// =================================================================
const float    HR_MIN_BPM             = 40.0f;   // intervalo máximo 1500 ms
const float    HR_MAX_BPM             = 180.0f;  // intervalo mínimo 333 ms
const float    HR_ENV_TAU_S           = 2.0f;    // caída de la envolvente
const float    HR_THRESHOLD_FRACTION  = 0.50f;   // umbral = 50 % de la envolvente
const float    HR_PROMINENCE_FRACTION = 0.50f;   // subida mínima desde el valle
const float    HR_MIN_ENVELOPE        = 30.0f;   // por debajo: señal débil (cuentas)
const uint32_t HR_REFRACTORY_MIN_MS   = 300;     // periodo refractario mínimo
const float    HR_REFRACTORY_FRACTION = 0.60f;   // o 60 % del intervalo medio
const uint32_t HR_WARMUP_SAMPLES      = 150;     // 1.5 s para medir la envolvente
const float    HR_INTERVAL_TOLERANCE  = 0.30f;   // desviación máx. vs mediana
const int      HR_HISTORY_SIZE        = 8;       // intervalos para el BPM estable
const int      HR_MIN_CONSECUTIVE     = 5;       // aceptados seguidos para VÁLIDO
const int      HR_LOSE_VALID_REJECTS  = 2;       // rechazos seguidos: deja VÁLIDO
const int      HR_MAX_CONSEC_REJECTS  = 3;       // rechazos seguidos: borra historial
const float    HR_MAX_CV              = 0.20f;   // variación máx. de intervalos (VÁLIDO)
const uint32_t HR_TIMEOUT_MIN_MS      = 3000;    // sin pulsos: pulso perdido
const float    HR_TIMEOUT_FACTOR      = 2.5f;    // o 2.5 x intervalo medio
const uint32_t HR_SAT_HOLD_SAMPLES    = 100;     // señal inválida 1 s tras saturar

const int      HR_NO_SIGNAL           = 0;
const int      HR_STABILIZING         = 1;
const int      HR_VALID               = 2;

// ---- Prueba guiada de BPM ----
const float    BPM_TEST_MIN_ENVELOPE  = 50.0f;   // amplitud PPG media mínima
const uint32_t BPM_TEST_MAX_TO_VALID_MS   = 15000;
const uint32_t BPM_TEST_MAX_TO_NOSIG_MS   = 3000;
const int      BPM_TEST_MIN_ACCEPTED  = 10;
const float    BPM_TEST_MAX_REJECT_FRACTION = 0.20f;
const float    BPM_TEST_MIN_VALID_FRACTION  = 0.80f;
const float    BPM_TEST_MIN_NOSIG_FRACTION  = 0.95f;

// =================================================================
//  ESTADO GLOBAL
// =================================================================
MAX30105 particleSensor;

bool     sensorReady      = false;
uint8_t  ledCurrent       = LED_CURRENT_DEFAULT;
char     outputMode       = 'b';   // Etapa 5: inicia en el monitor de BPM
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
char     modeBeforeTest    = 'b';

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

// ---- Frecuencia de muestreo medida (Etapa 5) ----
float    measuredFs        = FS_NOMINAL;   // muestras/s reales del sensor
uint32_t fsRefMs           = 0;
uint32_t fsRefSamples      = 0;
uint32_t fsRefLost         = 0;

// ---- Detector de pulsos (Etapa 5) ----
float    hrEnvDecay        = 0.0f;   // factor de caída de la envolvente por muestra
float    hrEnv             = 0.0f;   // envolvente de la PPG IR
float    hrThreshold       = 0.0f;   // umbral adaptativo actual
float    hrY1 = 0.0f, hrY2 = 0.0f;   // dos muestras previas de la PPG IR
uint32_t hrSamplesSeen     = 0;
uint32_t hrWarmup          = 0;
// Búsqueda del valle previo al candidato (corregido): se guardan las
// últimas HR_VALLEY_WINDOW muestras de la PPG IR y el valle se calcula
// solo entre el último pulso confirmado y el candidato actual.
const int HR_VALLEY_WINDOW = 160;    // 1.6 s: cubre un ciclo completo a 40 BPM
const float HR_VALLEY_INTERVAL_FRACTION = 1.1f; // con historial: 1.1 intervalos medios
float    hrPpgBuf[HR_VALLEY_WINDOW]; // PPG IR reciente (búfer circular)
uint32_t hrBufStartIdx     = 0;      // primera muestra válida del búfer
uint32_t hrValleyStartIdx  = 0;      // inicio de la búsqueda del valle del ciclo actual
bool     hrBufEmpty        = true;   // true tras un reinicio, hasta guardar la 1.a muestra
bool     hrPendingActive   = false;  // pico candidato en espera de confirmación
double   hrPendingT        = 0.0;    // tiempo del candidato (en muestras)
float    hrPendingValue    = 0.0f;
bool     hrHaveLastPulse   = false;
double   hrLastPulseT      = 0.0;    // tiempo del último pulso confirmado (muestras)
float    hrPeakMarker      = 0.0f;   // para el Serial Plotter
float    hrHistory[HR_HISTORY_SIZE]; // intervalos aceptados (ms)
int      hrHistCount       = 0;
int      hrHistPos         = 0;
float    hrHistCv          = 0.0f;
int      hrConsecAccepted  = 0;
int      hrConsecRejects   = 0;
int      hrState           = HR_NO_SIGNAL;
int      hrPrevState       = -1;
const char *hrReason       = "sin dedo";
uint32_t hrSatHold         = 0;
uint32_t hrLostSeen        = 0;
uint32_t hrPulses          = 0;      // pulsos confirmados (acumulado)
uint32_t hrAccepted        = 0;      // intervalos aceptados (acumulado)
uint32_t hrRejected        = 0;      // intervalos rechazados (acumulado)
uint32_t hrTimeouts        = 0;      // pulsos perdidos por tiempo
float    hrLastIntervalMs  = 0.0f;
float    hrLastBpmInst     = 0.0f;
bool     hrLastAcceptedFlag = false;
const char *hrLastRejectReason = "";
uint32_t hrLastPulseMs     = 0;      // millis() del último pulso confirmado

// ---- Prueba guiada de BPM ----
const int BT_IDLE           = 0;
const int BT_BASE_WAIT      = 1;
const int BT_BASE_MEASURE   = 2;
const int BT_PLACE          = 3;
const int BT_STILL          = 4;
const int BT_REMOVE_WAIT    = 5;
const int BT_REMOVE_MEASURE = 6;

const uint32_t BT_BASE_WAIT_MS      = 3000;
const uint32_t BT_BASE_MEASURE_MS   = 5000;
const uint32_t BT_PLACE_MS          = 10000;
const uint32_t BT_STILL_MS          = 20000;
const uint32_t BT_REMOVE_WAIT_MS    = 3000;
const uint32_t BT_REMOVE_MEASURE_MS = 5000;

int      btPhase           = BT_IDLE;
uint32_t btPhaseStartMs    = 0;
uint32_t btStartMs         = 0;
uint32_t btStartSamples    = 0;
uint32_t btStartLost       = 0;
char     btModeBefore      = 'd';
// Bloque sin dedo
uint32_t btBaseSamples = 0, btBaseNoSig = 0, btBaseAccepted = 0, btBasePulses = 0;
// Colocación
uint32_t btPlaceStartMs = 0;
long     btTimeToValidMs   = -1;
// Bloque con dedo quieto
uint32_t btStillSamples = 0, btStillValid = 0, btStillSat = 0;
uint32_t btStillAccepted = 0, btStillRejected = 0, btStillPulses = 0;
double   btStillEnvSum = 0.0;
double   btIntSum = 0.0, btIntSumSq = 0.0, btSuccDiffSq = 0.0;
float    btIntMin = 1e9f, btIntMax = 0.0f, btPrevInt = 0.0f;
uint32_t btIntCount = 0, btSuccCount = 0;
double   btBpmSum = 0.0;
uint32_t btBpmCount = 0;
float    btBpmMin = 1e9f, btBpmMax = 0.0f;
// Retiro
uint32_t btRemoveStartMs = 0;
long     btTimeToNoSigMs   = -1;
uint32_t btRemSamples = 0, btRemValid = 0, btRemNoSig = 0;

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
  } else if (outputMode == 'b') {
    printHeartRateSecond(nowMs, lostSamples - winLostStart, winSaturated);
  } else if (outputMode == 'o') {
    printSpo2Second(nowMs);
  }
  updateFsMeasurement(nowMs);          // frecuencia de muestreo real (Etapa 5)
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
//  MÓDULO: FRECUENCIA DE MUESTREO MEDIDA  (nuevo en la Etapa 5)
// =================================================================
// El oscilador interno del MAX30102 no es exacto (se midieron ~99.1
// muestras/s en lugar de 100). Como el intervalo entre pulsos se mide
// contando muestras, se usa la frecuencia REAL medida a largo plazo:
//   fs = (muestras recibidas + perdidas) / tiempo transcurrido
// Un error de 0.9 % en fs produciría un error de 0.9 % en el BPM.
void resetFsMeasurement(uint32_t nowMs) {
  fsRefMs      = nowMs;
  fsRefSamples = sampleIndex;
  fsRefLost    = lostSamples;
}

void updateFsMeasurement(uint32_t nowMs) {
  uint32_t elapsed = nowMs - fsRefMs;
  if (elapsed < 5000) return;                       // mínimo 5 s de datos
  uint32_t produced = (sampleIndex - fsRefSamples) + (lostSamples - fsRefLost);
  float fs = produced * 1000.0f / (float)elapsed;
  if (fs >= RATE_MIN && fs <= RATE_MAX) measuredFs = fs;
}

// =================================================================
//  MÓDULO: FRECUENCIA CARDIACA  (nuevo en la Etapa 5)
// =================================================================
// Entrada: PPG IR = -AC_IR (Etapa 4.1), pico sistólico hacia arriba.
//
// 1. Envolvente adaptativa: env = max(PPG, env * caída), tau = 2 s.
//    Sigue la altura de los picos recientes de ESTA persona y posición.
// 2. Umbral adaptativo = 50 % de la envolvente.
// 3. Máximo local: la pendiente cambia de positiva a negativa.
// 4. Prominencia: el pico debe subir >= 50 % de la envolvente desde el
//    valle de SU ciclo: el mínimo entre el pulso confirmado anterior y el
//    candidato, limitado a 1.1 intervalos medios (1.6 s sin historial).
//    Rechaza ondulaciones pequeñas y la onda dícrota, y nunca usa valles
//    de ciclos anteriores.
// 5. Periodo refractario = max(300 ms, 60 % del intervalo medio).
//    El candidato se CONFIRMA al terminar el periodo; si dentro de él
//    aparece un máximo más alto, lo reemplaza. Así hay un solo pulso
//    por latido y se conserva el verdadero pico sistólico.
// 6. Tiempo del pico con interpolación parabólica (resolución < 10 ms).
// 7. Intervalo = diferencia de tiempos de pulsos confirmados.

void hrComputeConstants() {
  hrEnvDecay = expf(-1.0f / (FS_NOMINAL * HR_ENV_TAU_S));
}

// Borra solo el historial de intervalos (mantiene la envolvente)
void hrClearHistory() {
  hrHistCount      = 0;
  hrHistPos        = 0;
  hrHistCv         = 0.0f;
  hrConsecAccepted = 0;
  hrConsecRejects  = 0;
}

// Reinicio completo del detector (sin dedo, saturación, reinicio de
// filtros o pérdida de muestras). Los contadores acumulados se conservan.
void hrResetDetector() {
  hrEnv           = 0.0f;
  hrThreshold     = 0.0f;
  hrY1            = 0.0f;
  hrY2            = 0.0f;
  hrSamplesSeen   = 0;
  hrWarmup        = HR_WARMUP_SAMPLES;
  hrBufEmpty      = true;           // el búfer del valle vuelve a empezar
  hrPendingActive = false;
  hrHaveLastPulse = false;
  hrClearHistory();
}

float hrMeanInterval() {
  if (hrHistCount == 0) return 0.0f;
  float s = 0.0f;
  for (int i = 0; i < hrHistCount; i++) s += hrHistory[i];
  return s / hrHistCount;
}

float hrMedianInterval() {
  if (hrHistCount == 0) return 0.0f;
  float tmp[HR_HISTORY_SIZE];
  for (int i = 0; i < hrHistCount; i++) tmp[i] = hrHistory[i];
  for (int i = 1; i < hrHistCount; i++) {          // ordenamiento por inserción
    float v = tmp[i];
    int j = i - 1;
    while (j >= 0 && tmp[j] > v) { tmp[j + 1] = tmp[j]; j--; }
    tmp[j + 1] = v;
  }
  if (hrHistCount % 2 == 1) return tmp[hrHistCount / 2];
  return 0.5f * (tmp[hrHistCount / 2 - 1] + tmp[hrHistCount / 2]);
}

// Coeficiente de variación de los intervalos del historial
float hrComputeCv() {
  if (hrHistCount < 2) return 0.0f;
  float mean = hrMeanInterval();
  float s = 0.0f;
  for (int i = 0; i < hrHistCount; i++) {
    float d = hrHistory[i] - mean;
    s += d * d;
  }
  return sqrtf(s / (hrHistCount - 1)) / mean;
}

// BPM estable = 60000 / media de los intervalos aceptados recientes
float hrStableBpm() {
  float m = hrMeanInterval();
  return (m > 0.0f) ? 60000.0f / m : 0.0f;
}

float hrRefractorySamples() {
  float ms = (float)HR_REFRACTORY_MIN_MS;
  if (hrHistCount > 0) {
    float adaptive = HR_REFRACTORY_FRACTION * hrMeanInterval();
    if (adaptive > ms) ms = adaptive;
  }
  return ms * measuredFs / 1000.0f;
}

float hrTimeoutSamples() {
  float ms = (float)HR_TIMEOUT_MIN_MS;
  if (hrHistCount > 0) {
    float adaptive = HR_TIMEOUT_FACTOR * hrMeanInterval();
    if (adaptive > ms) ms = adaptive;
  }
  return ms * measuredFs / 1000.0f;
}

// Desplazamiento (en muestras, entre -0.5 y 0.5) del vértice de la
// parábola que pasa por tres muestras consecutivas a, b (máximo), c.
float hrParabolicOffset(float a, float b, float c) {
  float den = a - 2.0f * b + c;
  if (den >= 0.0f) return 0.0f;                    // no es un máximo
  float d = 0.5f * (a - c) / den;
  if (d > 0.5f) d = 0.5f;
  if (d < -0.5f) d = -0.5f;
  return d;
}

// Valle del ciclo actual: mínimo de la PPG IR entre el inicio de la
// búsqueda (muestra siguiente al último pulso confirmado) y el candidato
// (peakIdx), limitado a 1.1 intervalos medios antes del candidato
// (o a 1.6 s si aún no hay historial de intervalos).
// Así la prominencia se mide respecto al valle de ESE ciclo y nunca
// respecto a mínimos de ciclos anteriores.
float hrValleyBefore(uint32_t peakIdx) {
  uint32_t maxBack = (uint32_t)(HR_VALLEY_WINDOW - 1);
  if (hrHistCount > 0) {
    uint32_t cycle = (uint32_t)(HR_VALLEY_INTERVAL_FRACTION * hrMeanInterval() * measuredFs / 1000.0f);
    if (cycle < maxBack) maxBack = cycle;
  }
  uint32_t start = hrValleyStartIdx;
  if (start < hrBufStartIdx) start = hrBufStartIdx;
  if (peakIdx >= maxBack && start < peakIdx - maxBack) start = peakIdx - maxBack;
  if (start > peakIdx) return hrPpgBuf[peakIdx % HR_VALLEY_WINDOW];
  float m = hrPpgBuf[start % HR_VALLEY_WINDOW];
  for (uint32_t i = start + 1; i <= peakIdx; i++) {
    float v = hrPpgBuf[i % HR_VALLEY_WINDOW];
    if (v < m) m = v;
  }
  return m;
}

const char *hrStateText(int state) {
  if (state == HR_VALID)       return "BPM VALIDO";
  if (state == HR_STABILIZING) return "ESTABILIZANDO";
  return "SIN SENAL";
}

// Evalúa un intervalo nuevo: aceptarlo o rechazarlo.
//  - Rango fisiológico razonable: 40-180 BPM (333-1500 ms).
//  - Consistencia: |I - mediana| / mediana <= 30 %. Un intervalo muy
//    largo suele ser un pulso perdido; uno muy corto, un pulso falso.
//  - 3 rechazos seguidos borran el historial (el ritmo de referencia
//    ya no es confiable) y el algoritmo vuelve a arrancar.
void hrEvaluateInterval(float intervalMs) {
  float minMs = 60000.0f / HR_MAX_BPM;
  float maxMs = 60000.0f / HR_MIN_BPM;
  bool ok = true;
  hrLastRejectReason = "";

  hrLastIntervalMs = intervalMs;
  hrLastBpmInst    = 60000.0f / intervalMs;

  if (intervalMs < minMs || intervalMs > maxMs) {
    ok = false;
    hrLastRejectReason = "fuera de 40-180 BPM";
  } else if (hrHistCount > 0) {
    float ref = hrMedianInterval();
    float dev = fabsf(intervalMs - ref) / ref;
    if (dev > HR_INTERVAL_TOLERANCE) {
      ok = false;
      hrLastRejectReason = (intervalMs > ref) ? "largo: posible pulso perdido"
                                              : "corto: posible pulso falso";
    }
  }

  hrLastAcceptedFlag = ok;
  if (ok) {
    hrHistory[hrHistPos] = intervalMs;
    hrHistPos = (hrHistPos + 1) % HR_HISTORY_SIZE;
    if (hrHistCount < HR_HISTORY_SIZE) hrHistCount++;
    hrHistCv = hrComputeCv();
    hrAccepted++;
    hrConsecAccepted++;
    hrConsecRejects = 0;
  } else {
    hrRejected++;
    hrConsecRejects++;
    hrConsecAccepted = 0;
    if (hrConsecRejects >= HR_MAX_CONSEC_REJECTS) {
      hrClearHistory();
      hrLastRejectReason = "3 rechazos seguidos: historial reiniciado";
    }
  }
  bpmTestOnInterval(intervalMs, ok);
}

// Estado con histéresis:
//  - Entra a BPM VÁLIDO con >= 5 intervalos aceptados SEGUIDOS y
//    variación (CV) del historial <= 20 %.
//  - Sale de BPM VÁLIDO con 2 rechazos seguidos, historial insuficiente
//    o CV > 20 %.
void hrUpdateState() {
  bool enoughHistory = (hrHistCount >= HR_MIN_CONSECUTIVE) && (hrHistCv <= HR_MAX_CV);
  if (hrState == HR_VALID) {
    if (hrConsecRejects >= HR_LOSE_VALID_REJECTS || !enoughHistory) {
      hrState = HR_STABILIZING;
      hrReason = "intervalos inconsistentes";
    }
  } else {
    if (hrConsecAccepted >= HR_MIN_CONSECUTIVE && enoughHistory) {
      hrState = HR_VALID;
      hrReason = "";
    } else {
      hrState = HR_STABILIZING;
      hrReason = "acumulando pulsos consistentes";
    }
  }
}

void hrPrintPulseLine() {
  if (outputMode != 'b') return;
  float tSec = hrLastPulseMs / 1000.0f;             // hora real del pico
  Serial.printf("  PULSO #%lu | t=%.2f s | ", (unsigned long)hrPulses, tSec);
  if (hrLastIntervalMs <= 0.0f) {
    Serial.print("primer pulso (sin intervalo)");
  } else if (hrLastAcceptedFlag) {
    Serial.printf("intervalo %.0f ms | BPM inst %.1f | ACEPTADO", hrLastIntervalMs, hrLastBpmInst);
  } else {
    Serial.printf("intervalo %.0f ms | BPM inst %.1f | RECHAZADO (%s)",
                  hrLastIntervalMs, hrLastBpmInst, hrLastRejectReason);
  }
  if (hrState == HR_VALID) Serial.printf(" | BPM estable %.1f\n", hrStableBpm());
  else if (hrHistCount > 0) Serial.printf(" | provisional %.1f (%s)\n", hrStableBpm(), hrStateText(hrState));
  else Serial.printf(" | %s\n", hrStateText(hrState));
}

// Confirma un pulso (fin del periodo refractario del candidato)
void hrCommitPulse(double tPeak) {
  hrPulses++;
  // Hora real del pico: la confirmación ocurre un periodo refractario
  // después, así que se resta el tiempo transcurrido desde el pico.
  double agoMs = ((double)sampleIndex - tPeak) * 1000.0 / measuredFs;
  hrLastPulseMs = millis() - (uint32_t)agoMs;
  hrLastIntervalMs = 0.0f;
  if (hrHaveLastPulse) {
    float intervalMs = (float)((tPeak - hrLastPulseT) * 1000.0 / measuredFs);
    hrEvaluateInterval(intervalMs);
    hrUpdateState();
    spo2OnBeat(hrLastPulseT, tPeak, hrLastAcceptedFlag);   // Etapa 6.1: R por latido
  }
  hrLastPulseT    = tPeak;
  hrHaveLastPulse = true;
  // El valle del SIGUIENTE ciclo se busca a partir de la muestra que sigue
  // al pico confirmado (el búfer conserva las muestras entre el pico y la
  // confirmación, así que no se pierde parte del descenso).
  hrValleyStartIdx = (uint32_t)(tPeak + 0.5) + 1;
  bpmTestOnPulse();
  hrPrintPulseLine();
}

void hrSetNoSignal(const char *reason) {
  hrState  = HR_NO_SIGNAL;
  hrReason = reason;
}

// Se llama con CADA muestra, después de processPPG().
void updateHeartRate(uint32_t red, uint32_t ir) {
  hrPeakMarker = 0.0f;

  bool saturated = (red >= SATURATION_LEVEL || ir >= SATURATION_LEVEL);
  if (saturated) hrSatHold = HR_SAT_HOLD_SAMPLES;
  else if (hrSatHold > 0) hrSatHold--;
  bool lostNow = (lostSamples != hrLostSeen);
  hrLostSeen = lostSamples;
  bool finger = (lastDc[CH_IR] >= (float)FINGER_IR_THRESHOLD);

  // --- Condiciones que invalidan la señal ---
  if (!finger) {
    hrResetDetector();
    hrSetNoSignal("sin dedo");
    return;
  }
  if (hrSatHold > 0) {
    hrResetDetector();
    hrSetNoSignal("saturacion del ADC");
    return;
  }
  // Reinicio de filtros o muestras perdidas: la base de tiempo o la
  // señal cambiaron; el detector vuelve a empezar.
  if (resetThisSample || lostNow) hrResetDetector();
  if (!filtersSettled()) {
    hrState  = HR_STABILIZING;
    hrReason = "filtros estabilizando";
    return;
  }

  float y = -lastAc[CH_IR];                        // PPG IR, pico hacia arriba
  double tNow = (double)sampleIndex;               // tiempo en muestras

  // --- Búfer de la PPG para buscar el valle de cada ciclo ---
  if (hrBufEmpty) {
    hrBufStartIdx    = sampleIndex;
    hrValleyStartIdx = sampleIndex;
    hrBufEmpty       = false;
  }
  hrPpgBuf[sampleIndex % HR_VALLEY_WINDOW] = y;

  // --- Envolvente y umbral adaptativo ---
  float decayed = hrEnv * hrEnvDecay;
  hrEnv = (y > decayed) ? y : decayed;
  hrThreshold = HR_THRESHOLD_FRACTION * hrEnv;

  if (hrWarmup > 0) {                              // 1.5 s: solo se mide la envolvente
    hrWarmup--;
    hrY2 = hrY1;  hrY1 = y;  hrSamplesSeen++;
    hrState  = HR_STABILIZING;
    hrReason = "midiendo amplitud";
    return;
  }
  if (hrEnv < HR_MIN_ENVELOPE) {
    hrClearHistory();
    hrPendingActive = false;
    hrHaveLastPulse = false;
    hrY2 = hrY1;  hrY1 = y;
    hrSetNoSignal("senal debil");
    return;
  }

  // --- Confirmación del candidato al terminar su periodo refractario ---
  if (hrPendingActive && (tNow - hrPendingT) >= hrRefractorySamples()) {
    hrPendingActive = false;
    hrCommitPulse(hrPendingT);
  }

  // --- Máximo local en la muestra anterior (pendiente + a -) ---
  if (hrSamplesSeen >= 2 && hrY1 > hrY2 && hrY1 >= y) {
    double tPeak = (tNow - 1.0) + hrParabolicOffset(hrY2, hrY1, y);
    if (hrPendingActive) {
      // Dentro del periodo refractario: conservar el máximo más alto
      if (hrY1 > hrPendingValue) {
        hrPendingT     = tPeak;
        hrPendingValue = hrY1;
        hrPeakMarker   = hrY1;
      }
    } else {
      bool aboveThreshold = (hrY1 >= hrThreshold);
      float valley        = hrValleyBefore(sampleIndex - 1);  // valle de ESTE ciclo
      bool prominent      = ((hrY1 - valley) >= HR_PROMINENCE_FRACTION * hrEnv);
      bool refractoryOk   = !hrHaveLastPulse ||
                            ((tPeak - hrLastPulseT) >= hrRefractorySamples());
      if (aboveThreshold && prominent && refractoryOk) {
        hrPendingActive = true;
        hrPendingT      = tPeak;
        hrPendingValue  = hrY1;
        hrPeakMarker    = hrY1;
      }
    }
  }

  // --- Pulso perdido: demasiado tiempo sin pulsos confirmados ---
  if (hrHaveLastPulse && !hrPendingActive && (tNow - hrLastPulseT) > hrTimeoutSamples()) {
    hrTimeouts++;
    hrClearHistory();
    hrHaveLastPulse = false;
    if (outputMode == 'b') Serial.println("  >> Sin pulsos durante demasiado tiempo: historial reiniciado");
  }

  hrY2 = hrY1;
  hrY1 = y;
  hrSamplesSeen++;
  hrUpdateState();
}

void printHeartRateConfiguration() {
  Serial.println("  Deteccion de pulsos (PPG IR):");
  Serial.printf ("    Envolvente adaptativa tau = %.1f s | umbral = %.0f %% de la envolvente\n",
                 HR_ENV_TAU_S, HR_THRESHOLD_FRACTION * 100.0f);
  Serial.printf ("    Prominencia minima = %.0f %% de la envolvente | amplitud minima = %.0f cuentas\n",
                 HR_PROMINENCE_FRACTION * 100.0f, HR_MIN_ENVELOPE);
  Serial.printf ("    Periodo refractario = max(%lu ms, %.0f %% del intervalo medio)\n",
                 (unsigned long)HR_REFRACTORY_MIN_MS, HR_REFRACTORY_FRACTION * 100.0f);
  Serial.printf ("    Intervalos aceptados: %.0f-%.0f BPM y <= %.0f %% de la mediana reciente\n",
                 HR_MIN_BPM, HR_MAX_BPM, HR_INTERVAL_TOLERANCE * 100.0f);
  Serial.printf ("    BPM VALIDO: >= %d intervalos aceptados seguidos y CV <= %.0f %%\n",
                 HR_MIN_CONSECUTIVE, HR_MAX_CV * 100.0f);
  Serial.printf ("    Frecuencia de muestreo medida: %.2f muestras/s\n\n", measuredFs);
}

void printHeartRateSecond(uint32_t nowMs, uint32_t winLost, uint32_t winSat) {
  if (hrPrevState != hrState) {
    Serial.printf("  >> Estado: %s\n", hrStateText(hrState));
    hrPrevState = hrState;
  }
  Serial.printf("== t=%lus | %s", (unsigned long)(nowMs / 1000UL), hrStateText(hrState));
  if (hrState == HR_VALID) Serial.printf(" | BPM estable %.1f", hrStableBpm());
  else if (hrReason[0] != '\0') Serial.printf(" (%s)", hrReason);
  if (hrPulses > 0 && hrHaveLastPulse) {
    Serial.printf(" | ultimo pulso hace %.1f s", (nowMs - hrLastPulseMs) / 1000.0f);
  }
  Serial.printf(" | pulsos %lu (acept %lu, rech %lu)\n",
                (unsigned long)hrPulses, (unsigned long)hrAccepted, (unsigned long)hrRejected);
  Serial.printf("   PPG IR %.1f | envolvente %.1f | umbral %.1f | CV intervalos %.0f %% | "
                "fs %.2f | perdidas %lu | saturadas %lu\n",
                -lastAc[CH_IR], hrEnv, hrThreshold, hrHistCv * 100.0f, measuredFs,
                (unsigned long)winLost, (unsigned long)winSat);
}

// =================================================================
//  MÓDULO: PRUEBA GUIADA DE BPM  (nuevo en la Etapa 5)
// =================================================================
void bpmTestReset() {
  btBaseSamples = 0; btBaseNoSig = 0; btBaseAccepted = 0; btBasePulses = 0;
  btTimeToValidMs = -1;
  btStillSamples = 0; btStillValid = 0; btStillSat = 0;
  btStillAccepted = 0; btStillRejected = 0; btStillPulses = 0;
  btStillEnvSum = 0.0;
  btIntSum = 0.0; btIntSumSq = 0.0; btSuccDiffSq = 0.0;
  btIntMin = 1e9f; btIntMax = 0.0f; btPrevInt = 0.0f;
  btIntCount = 0; btSuccCount = 0;
  btBpmSum = 0.0; btBpmCount = 0; btBpmMin = 1e9f; btBpmMax = 0.0f;
  btTimeToNoSigMs = -1;
  btRemSamples = 0; btRemValid = 0; btRemNoSig = 0;
}

// Llamada por hrCommitPulse() en cada pulso confirmado
void bpmTestOnPulse() {
  if (btPhase == BT_BASE_MEASURE) btBasePulses++;
  if (btPhase == BT_STILL)        btStillPulses++;
}

// Llamada por hrEvaluateInterval() en cada intervalo evaluado
void bpmTestOnInterval(float intervalMs, bool accepted) {
  if (btPhase == BT_BASE_MEASURE && accepted) btBaseAccepted++;
  if (btPhase != BT_STILL) return;
  if (!accepted) { btStillRejected++; btPrevInt = 0.0f; return; }
  btStillAccepted++;
  btIntSum   += intervalMs;
  btIntSumSq += (double)intervalMs * intervalMs;
  btIntCount++;
  if (intervalMs < btIntMin) btIntMin = intervalMs;
  if (intervalMs > btIntMax) btIntMax = intervalMs;
  if (btPrevInt > 0.0f) {                  // diferencias sucesivas (informativo)
    float d = intervalMs - btPrevInt;
    btSuccDiffSq += (double)d * d;
    btSuccCount++;
  }
  btPrevInt = intervalMs;
}

// Llamada con cada muestra (después de updateHeartRate)
void bpmTestOnSample(uint32_t nowMs, uint32_t red, uint32_t ir) {
  if (btPhase == BT_IDLE) return;
  bool sat = (red >= SATURATION_LEVEL || ir >= SATURATION_LEVEL);

  if (btPhase == BT_BASE_MEASURE) {
    btBaseSamples++;
    if (hrState == HR_NO_SIGNAL) btBaseNoSig++;
  }
  if ((btPhase == BT_PLACE || btPhase == BT_STILL) && btTimeToValidMs < 0 && hrState == HR_VALID) {
    btTimeToValidMs = (long)(nowMs - btPlaceStartMs);
  }
  if (btPhase == BT_STILL) {
    btStillSamples++;
    btStillEnvSum += hrEnv;
    if (sat) btStillSat++;
    if (hrState == HR_VALID) {
      btStillValid++;
      float bpm = hrStableBpm();
      btBpmSum += bpm;
      btBpmCount++;
      if (bpm < btBpmMin) btBpmMin = bpm;
      if (bpm > btBpmMax) btBpmMax = bpm;
    }
  }
  if ((btPhase == BT_REMOVE_WAIT || btPhase == BT_REMOVE_MEASURE) &&
      btTimeToNoSigMs < 0 && hrState == HR_NO_SIGNAL) {
    btTimeToNoSigMs = (long)(nowMs - btRemoveStartMs);
  }
  if (btPhase == BT_REMOVE_MEASURE) {
    btRemSamples++;
    if (hrState == HR_VALID) btRemValid++;
    if (hrState == HR_NO_SIGNAL) btRemNoSig++;
  }
}

void startBpmTest(uint32_t nowMs) {
  if (!sensorReady) {
    Serial.println(">> El sensor no esta listo; no se puede iniciar la prueba.");
    return;
  }
  bpmTestReset();
  btModeBefore   = outputMode;
  outputMode     = 'b';
  btStartMs      = nowMs;
  btStartSamples = sampleIndex;
  btStartLost    = lostSamples;
  btPhase        = BT_BASE_WAIT;
  btPhaseStartMs = nowMs;
  hrPrevState    = -1;

  Serial.println("\n================================================");
  Serial.println(" PRUEBA GUIADA DE BPM (aprox. 46 s)");
  Serial.println(" Estimacion experimental. NO es una medicion clinica.");
  Serial.println(" Opcional: compare con un oximetro comercial o contando");
  Serial.println(" el pulso radial durante 30 s x 2.");
  Serial.println("================================================");
  Serial.println(">> FASE 1: RETIRE el dedo y aleje la mano del sensor (> 10 cm).");
  Serial.println("   (3 s de espera + 5 s de medicion)");
}

void finishBpmTest(uint32_t nowMs) {
  uint32_t testSamples = sampleIndex - btStartSamples;
  uint32_t testLost    = lostSamples - btStartLost;
  float    testRate    = testSamples * 1000.0f / (float)(nowMs - btStartMs);

  float baseNoSigFrac = (btBaseSamples > 0) ? (float)btBaseNoSig / btBaseSamples : 0.0f;
  float validFrac     = (btStillSamples > 0) ? (float)btStillValid / btStillSamples : 0.0f;
  uint32_t evaluated  = btStillAccepted + btStillRejected;
  float rejectFrac    = (evaluated > 0) ? (float)btStillRejected / evaluated : 1.0f;
  float meanEnv       = (btStillSamples > 0) ? (float)(btStillEnvSum / btStillSamples) : 0.0f;
  float meanInt = 0.0f, sdInt = 0.0f, cvInt = 0.0f, rmssd = 0.0f;
  if (btIntCount > 0) meanInt = (float)(btIntSum / btIntCount);
  if (btIntCount > 1) {
    double var = (btIntSumSq - btIntSum * btIntSum / btIntCount) / (btIntCount - 1);
    sdInt = (var > 0.0) ? (float)sqrt(var) : 0.0f;
    cvInt = (meanInt > 0.0f) ? sdInt / meanInt : 0.0f;
  }
  if (btSuccCount > 0) rmssd = (float)sqrt(btSuccDiffSq / btSuccCount);
  float meanBpm = (btBpmCount > 0) ? (float)(btBpmSum / btBpmCount) : 0.0f;
  float remValidFrac = (btRemSamples > 0) ? (float)btRemValid / btRemSamples : 1.0f;

  bool ok1  = (btBaseAccepted == 0) && (baseNoSigFrac >= BPM_TEST_MIN_NOSIG_FRACTION);
  bool ok2  = (btTimeToValidMs >= 0) && (btTimeToValidMs <= (long)BPM_TEST_MAX_TO_VALID_MS);
  bool ok3  = (btStillAccepted >= (uint32_t)BPM_TEST_MIN_ACCEPTED);
  bool ok4  = (evaluated > 0) && (rejectFrac <= BPM_TEST_MAX_REJECT_FRACTION);
  bool ok5  = (validFrac >= BPM_TEST_MIN_VALID_FRACTION);
  bool ok6  = (btIntCount >= 2) && (cvInt <= HR_MAX_CV);
  bool ok7  = (btBpmCount > 0) && (btBpmMin >= HR_MIN_BPM) && (btBpmMax <= HR_MAX_BPM);
  bool ok8  = (meanEnv >= BPM_TEST_MIN_ENVELOPE);
  bool ok9  = (btTimeToNoSigMs >= 0) && (btTimeToNoSigMs <= (long)BPM_TEST_MAX_TO_NOSIG_MS) &&
              (remValidFrac == 0.0f);
  bool ok10 = (testLost == 0);
  bool ok11 = (btStillSat == 0);
  bool ok12 = (testRate >= RATE_MIN && testRate <= RATE_MAX);

  Serial.println("\n================================================");
  Serial.println(" RESULTADOS DE LA PRUEBA DE BPM (estimacion experimental)");
  Serial.println("================================================");
  Serial.printf ("  Sin dedo      : SIN SENAL %.0f %% del tiempo | pulsos %lu | intervalos aceptados %lu\n",
                 baseNoSigFrac * 100.0f, (unsigned long)btBasePulses, (unsigned long)btBaseAccepted);
  if (btTimeToValidMs >= 0)
    Serial.printf("  Colocacion    : BPM VALIDO a los %.1f s de colocar el dedo\n", btTimeToValidMs / 1000.0f);
  else
    Serial.println("  Colocacion    : nunca se alcanzo BPM VALIDO");
  Serial.printf ("  Dedo quieto   : %lu pulsos | intervalos aceptados %lu | rechazados %lu (%.0f %%)\n",
                 (unsigned long)btStillPulses, (unsigned long)btStillAccepted,
                 (unsigned long)btStillRejected, rejectFrac * 100.0f);
  Serial.printf ("                  BPM VALIDO %.0f %% del tiempo | envolvente PPG media %.1f cuentas\n",
                 validFrac * 100.0f, meanEnv);
  if (btIntCount > 0) {
    Serial.printf("                  intervalo medio %.0f ms (DE %.0f ms, CV %.1f %%, min %.0f, max %.0f)\n",
                  meanInt, sdInt, cvInt * 100.0f, btIntMin, btIntMax);
    Serial.printf("                  RMSSD %.0f ms (informativo)\n", rmssd);
  }
  if (btBpmCount > 0)
    Serial.printf("                  BPM estable: media %.1f | min %.1f | max %.1f\n", meanBpm, btBpmMin, btBpmMax);
  if (btTimeToNoSigMs >= 0)
    Serial.printf("  Retiro        : SIN SENAL a los %.1f s | BPM VALIDO en medicion final: %.0f %%\n",
                  btTimeToNoSigMs / 1000.0f, remValidFrac * 100.0f);
  else
    Serial.println("  Retiro        : nunca paso a SIN SENAL");
  Serial.printf ("  Adquisicion   : %.1f muestras/s (fs medida %.2f) | perdidas %lu | saturadas %lu\n",
                 testRate, measuredFs, (unsigned long)testLost, (unsigned long)btStillSat);
  Serial.println("------------------------------------------------");
  printTestResult("1. Sin dedo: 0 intervalos aceptados y SIN SENAL >= 95 %", ok1);
  printTestResult("2. BPM VALIDO en <= 15 s tras colocar el dedo", ok2);
  printTestResult("3. >= 10 intervalos aceptados con el dedo quieto (20 s)", ok3);
  printTestResult("4. Intervalos rechazados <= 20 %", ok4);
  printTestResult("5. Estado BPM VALIDO >= 80 % del tiempo con dedo quieto", ok5);
  printTestResult("6. Variacion de intervalos aceptados (CV) <= 20 %", ok6);
  printTestResult("7. BPM estable siempre dentro de 40-180", ok7);
  printTestResult("8. Amplitud PPG suficiente (envolvente media >= 50)", ok8);
  printTestResult("9. SIN SENAL <= 3 s tras retirar el dedo y sin BPM despues", ok9);
  printTestResult("10. Sin muestras perdidas", ok10);
  printTestResult("11. Sin saturacion", ok11);
  printTestResult("12. Tasa de muestreo entre 95 y 105 muestras/s", ok12);
  Serial.println("------------------------------------------------");

  if (ok1 && ok2 && ok3 && ok4 && ok5 && ok6 && ok7 && ok8 && ok9 && ok10 && ok11 && ok12) {
    Serial.println(" RESULTADO: PRUEBA DE BPM SUPERADA");
    Serial.printf (" BPM estable medio: %.1f (estimacion experimental, sin validez clinica)\n", meanBpm);
    Serial.println(" Para comparar: cuente su pulso radial 30 s y multiplique x 2,");
    Serial.println(" o use un oximetro comercial al mismo tiempo en otra mano.");
  } else {
    Serial.println(" RESULTADO: PRUEBA DE BPM NO SUPERADA. Sugerencias:");
    if (!ok1) Serial.println("  - Fase sin dedo: aleje por completo la mano del sensor.");
    if (!ok2 || !ok3 || !ok5) Serial.println("  - Pocos pulsos validos: apoye el dedo sin presionar y no lo mueva.");
    if (!ok4 || !ok6) Serial.println("  - Intervalos irregulares: movimiento, o ritmo irregular (el algoritmo lo rechaza).");
    if (!ok8) Serial.println("  - Amplitud baja: caliente la mano o reposicione el dedo.");
    if (!ok9) Serial.println("  - Retiro: aleje el dedo por completo al final.");
    if (!ok10 || !ok12) Serial.println("  - Problema de adquisicion: revise el diagnostico ('d').");
    if (!ok11) Serial.println("  - Saturacion: presione menos o reduzca la corriente con '-'.");
  }
  Serial.println("================================================\n");

  btPhase    = BT_IDLE;
  outputMode = btModeBefore;
}

void updateBpmTest(uint32_t nowMs) {
  if (btPhase == BT_IDLE) return;
  uint32_t elapsed = nowMs - btPhaseStartMs;

  if (btPhase == BT_BASE_WAIT && elapsed >= BT_BASE_WAIT_MS) {
    btPhase = BT_BASE_MEASURE;
    btPhaseStartMs = nowMs;
    Serial.println(">> Midiendo SIN dedo (5 s)...");
  } else if (btPhase == BT_BASE_MEASURE && elapsed >= BT_BASE_MEASURE_MS) {
    btPhase = BT_PLACE;
    btPhaseStartMs = nowMs;
    btPlaceStartMs = nowMs;
    Serial.println(">> FASE 2: COLOQUE la yema del dedo sobre el sensor, SIN presionar.");
    Serial.println("   Apoye la mano en la mesa y no se mueva. (10 s de estabilizacion)");
  } else if (btPhase == BT_PLACE && elapsed >= BT_PLACE_MS) {
    btPhase = BT_STILL;
    btPhaseStartMs = nowMs;
    Serial.println(">> FASE 3: MANTENGA el dedo QUIETO. Midiendo 20 s...");
  } else if (btPhase == BT_STILL && elapsed >= BT_STILL_MS) {
    btPhase = BT_REMOVE_WAIT;
    btPhaseStartMs = nowMs;
    btRemoveStartMs = nowMs;
    Serial.println(">> FASE 4: RETIRE el dedo y aleje la mano (> 10 cm).");
    Serial.println("   (3 s de espera + 5 s de medicion)");
  } else if (btPhase == BT_REMOVE_WAIT && elapsed >= BT_REMOVE_WAIT_MS) {
    btPhase = BT_REMOVE_MEASURE;
    btPhaseStartMs = nowMs;
    Serial.println(">> Midiendo SIN dedo nuevamente (5 s)...");
  } else if (btPhase == BT_REMOVE_MEASURE && elapsed >= BT_REMOVE_MEASURE_MS) {
    finishBpmTest(nowMs);
  }
}

// =================================================================
//  MÓDULO: CÁLCULO DE R POR LATIDO  (nuevo en la Etapa 6.1)
// =================================================================
// R = (AC_RED / DC_RED) / (AC_IR / DC_IR)   ("razón de razones")
//
//  - Se calcula UNA vez por latido, solo en intervalos ACEPTADOS por el
//    detector de BPM validado, sobre las muestras entre dos picos
//    confirmados consecutivos (un ciclo cardiaco completo).
//  - Estimador PRINCIPAL (R): pendiente de la regresión de AC_RED contra
//    AC_IR dentro del latido, multiplicada por DC_IR/DC_RED. Equivale a
//    AC_RED/AC_IR, pero el ruido del canal rojo (no correlacionado con el
//    IR) se promedia en lugar de inflar la amplitud.
//  - Estimadores de COMPARACIÓN: pico a pico (R_pp) y RMS (R_rms). En
//    simulación, con ruido, R_pp se sesgó hasta +17 % y R_rms hasta +10 %,
//    mientras que la regresión se mantuvo dentro de +-1 %.
//  - Verificación cruzada (diseño de la etapa): |R_pp - R_rms| / R_pp
//    <= 15 %. Una discrepancia grande indica ruido o artefactos en el
//    latido.
//  - DC de cada canal: promedio del estimador DC durante el latido.
//  - Dividir entre el DC normaliza la intensidad de cada LED y la
//    sensibilidad del fotodiodo; como ambos canales pasan por filtros
//    idénticos, la ganancia del filtro se cancela en el cociente.
//
//  En esta etapa R NO se convierte a SpO2. Se calculan también métricas
//  de calidad (perfusión, correlación RED-IR, estabilidad del DC) que en
//  la Etapa 6.2 servirán para aceptar o descartar latidos.

const int      SP_BUF_SIZE       = 256;    // ~2.6 s: latido de 1.5 s + retraso de confirmación
const int      SP_R_HISTORY      = 8;      // valores de R para la mediana
// Umbrales preliminares (solo informativos en la 6.1; se aplicarán en la 6.2)
const float    SP_MIN_PI_IR      = 0.20f;  // índice de perfusión IR mínimo (%)
const float    SP_MIN_ACPP_RED   = 50.0f;  // AC pico a pico RED mínimo (cuentas)
const float    SP_MIN_CORR       = 0.80f;  // correlación RED-IR mínima del latido
const float    SP_MAX_R_DIFF     = 0.15f;  // |R_pp - R_rms| / R_pp máximo (verificación cruzada)
const float    SP_MAX_DC_CHANGE  = 0.05f;  // cambio del DC dentro del latido (fracción)
const float    SP_R_MIN_USEFUL   = 0.34f;  // zona monótona de la curva de Maxim
const float    SP_R_MAX_USEFUL   = 1.15f;

// ---- Búfer circular de AC y DC de ambos canales ----
float    spAcRed[SP_BUF_SIZE];
float    spAcIr[SP_BUF_SIZE];
float    spDcRed[SP_BUF_SIZE];
float    spDcIr[SP_BUF_SIZE];
bool     spOk[SP_BUF_SIZE];                // filtros estables y sin saturación
uint32_t spValidSinceIdx   = 0;            // no usar muestras anteriores a un reinicio

// ---- Resultado del último latido ----
uint32_t spBeats           = 0;            // latidos con R calculado (acumulado)
uint32_t spSkipped         = 0;            // latidos aceptados sin R (ventana no válida)
float    spLastIntervalMs  = 0.0f;
float    spLastAcppRed = 0.0f, spLastAcppIr = 0.0f;
float    spLastRmsRed  = 0.0f, spLastRmsIr  = 0.0f;
float    spLastDcRed   = 0.0f, spLastDcIr   = 0.0f;
float    spLastRpp     = 0.0f, spLastRrms   = 0.0f;
float    spLastRdiff   = 0.0f;            // |R_pp - R_rms| / R_pp (verificación cruzada)
float    spLastRreg    = 0.0f;            // R por pendiente de regresión RED vs IR
float    spLastPiRed   = 0.0f, spLastPiIr   = 0.0f;
float    spLastCorr    = 0.0f, spLastDcChange = 0.0f;
bool     spLastQualityOk   = false;        // cumple los umbrales preliminares

// ---- Historial de R de REGRESIÓN (estimador principal) ----
float    spR[SP_R_HISTORY];
int      spRCount          = 0;
int      spRPos            = 0;

// ---- Prueba guiada de R ----
const int RT_IDLE           = 0;
const int RT_BASE_WAIT      = 1;
const int RT_BASE_MEASURE   = 2;
const int RT_PLACE          = 3;
const int RT_STILL          = 4;
const int RT_REMOVE_WAIT    = 5;
const int RT_REMOVE_MEASURE = 6;
const uint32_t RT_BASE_WAIT_MS      = 3000;
const uint32_t RT_BASE_MEASURE_MS   = 5000;
const uint32_t RT_PLACE_MS          = 10000;
const uint32_t RT_STILL_MS          = 30000;
const uint32_t RT_REMOVE_WAIT_MS    = 3000;
const uint32_t RT_REMOVE_MEASURE_MS = 5000;
const int      RT_MAX_BEATS         = 64;
const int      RT_MIN_BEATS         = 15;
const float    RT_MAX_CV_R          = 0.10f;

int      rtPhase           = RT_IDLE;
uint32_t rtPhaseStartMs    = 0;
uint32_t rtStartMs         = 0;
uint32_t rtStartSamples    = 0;
uint32_t rtStartLost       = 0;
char     rtModeBefore      = 'b';
uint32_t rtBaseBeats       = 0;            // latidos con R sin dedo (debe ser 0)
uint32_t rtRemBeats        = 0;            // latidos con R tras retirar (medición final)
uint32_t rtStillSat        = 0;
int      rtN               = 0;            // latidos del bloque quieto con BPM VÁLIDO
float    rtR[RT_MAX_BEATS];                // R principal (regresión)
float    rtRpp[RT_MAX_BEATS];              // R pico a pico (comparación)
float    rtRdiff[RT_MAX_BEATS];
float    rtPiIr[RT_MAX_BEATS];
float    rtAcRed[RT_MAX_BEATS];
float    rtCorr[RT_MAX_BEATS];
double   rtDcRedSum        = 0.0;
double   rtDcIrSum         = 0.0;
int      rtQualityOk       = 0;

// -----------------------------------------------------------------
// Guarda la muestra actual (después de processPPG). Una muestra es
// utilizable si los filtros están estables y no hay saturación.
void spo2StoreSample(uint32_t red, uint32_t ir) {
  if (resetThisSample) spValidSinceIdx = sampleIndex;   // no cruzar reinicios
  if (hrState == HR_NO_SIGNAL && spRCount > 0) spClearHistory();   // sin dedo: no conservar R antiguos
  int k = (int)(sampleIndex % SP_BUF_SIZE);
  spAcRed[k] = lastAc[CH_RED];
  spAcIr[k]  = lastAc[CH_IR];
  spDcRed[k] = lastDc[CH_RED];
  spDcIr[k]  = lastDc[CH_IR];
  spOk[k]    = filtersSettled() && (red < SATURATION_LEVEL) && (ir < SATURATION_LEVEL);
}

// Mediana de un arreglo pequeño (copia local, ordenamiento por inserción)
float spMedianOf(const float *v, int n) {
  if (n <= 0) return 0.0f;
  float tmp[RT_MAX_BEATS];
  if (n > RT_MAX_BEATS) n = RT_MAX_BEATS;
  for (int i = 0; i < n; i++) tmp[i] = v[i];
  for (int i = 1; i < n; i++) {
    float x = tmp[i];
    int j = i - 1;
    while (j >= 0 && tmp[j] > x) { tmp[j + 1] = tmp[j]; j--; }
    tmp[j + 1] = x;
  }
  if (n % 2 == 1) return tmp[n / 2];
  return 0.5f * (tmp[n / 2 - 1] + tmp[n / 2]);
}

float spCvOf(const float *v, int n) {
  if (n < 2) return 0.0f;
  float mean = 0.0f;
  for (int i = 0; i < n; i++) mean += v[i];
  mean /= n;
  if (mean <= 0.0f) return 0.0f;
  float s = 0.0f;
  for (int i = 0; i < n; i++) { float d = v[i] - mean; s += d * d; }
  return sqrtf(s / (n - 1)) / mean;
}

// Mediana y CV del historial de R de REGRESIÓN (estimador principal)
float spMedianR() { return spMedianOf(spR, spRCount); }
float spCvR()     { return spCvOf(spR, spRCount); }

void spClearHistory() {
  spRCount = 0;
  spRPos   = 0;
}

void spPrintBeatLine() {
  if (outputMode == 'o') {
    Serial.printf("  LATIDO #%lu | int %.0f ms | R reg %.3f | R pp %.3f, R rms %.3f (dif pp-rms %.1f %%) | PI IR %.2f %% RED %.2f %% | "
                  "ACpp RED %.0f IR %.0f | DC RED %.0f IR %.0f | corr %.2f | dDC %.1f %% | %s | mediana R reg(%d) %.3f\n",
                  (unsigned long)spBeats, spLastIntervalMs, spLastRreg, spLastRpp, spLastRrms, spLastRdiff * 100.0f,
                  spLastPiIr, spLastPiRed, spLastAcppRed, spLastAcppIr, spLastDcRed, spLastDcIr,
                  spLastCorr, spLastDcChange * 100.0f, spLastQualityOk ? "calidad OK" : "calidad baja",
                  spRCount, spMedianR());
  } else if (outputMode == 'c') {
    Serial.printf("%lu,%lu,%.0f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.4f,%.4f,%.4f,%.3f,%.3f,%.3f,%.2f,%d\n",
                  (unsigned long)spBeats, (unsigned long)hrLastPulseMs, spLastIntervalMs,
                  spLastAcppRed, spLastAcppIr, spLastDcRed, spLastDcIr, spLastRmsRed, spLastRmsIr,
                  spLastRreg, spLastRpp, spLastRrms, spLastPiIr, spLastPiRed, spLastCorr,
                  spLastDcChange * 100.0f, hrState);
  }
}

// Latido aceptado por el detector pero sin R calculable
void spSkipBeat(const char *reason) {
  spSkipped++;
  if (outputMode == 'o') Serial.printf("  LATIDO sin R: %s\n", reason);
}

// Llamada desde hrCommitPulse() en cada intervalo evaluado.
// tPrev y tPeak: tiempos (en muestras) de los dos picos que delimitan el latido.
void spo2OnBeat(double tPrev, double tPeak, bool accepted) {
  if (!accepted) return;                          // solo intervalos aceptados

  uint32_t i0 = (uint32_t)(tPrev + 0.5);
  uint32_t i1 = (uint32_t)(tPeak + 0.5);
  if (i1 <= i0 + 2)                              { spSkipBeat("latido demasiado corto"); return; }
  if (sampleIndex - i0 >= (uint32_t)SP_BUF_SIZE) { spSkipBeat("fuera del bufer"); return; }
  if (i0 < spValidSinceIdx)                      { spSkipBeat("cruza un reinicio de filtros"); return; }

  // Recorrido del latido: máximos, mínimos, sumas para RMS, DC y correlación
  float maxR = -1e9f, minR = 1e9f, maxI = -1e9f, minI = 1e9f;
  double sR = 0, sI = 0, sRR = 0, sII = 0, sRI = 0, dcR = 0, dcI = 0;
  uint32_t n = 0;
  for (uint32_t i = i0; i <= i1; i++) {
    int k = (int)(i % SP_BUF_SIZE);
    if (!spOk[k]) { spSkipBeat("muestras no validas en el latido"); return; }
    float r = spAcRed[k], a = spAcIr[k];
    if (r > maxR) maxR = r;
    if (r < minR) minR = r;
    if (a > maxI) maxI = a;
    if (a < minI) minI = a;
    sR += r; sI += a; sRR += (double)r * r; sII += (double)a * a; sRI += (double)r * a;
    dcR += spDcRed[k]; dcI += spDcIr[k];
    n++;
  }
  float dcRed = (float)(dcR / n);
  float dcIr  = (float)(dcI / n);
  float acppRed = maxR - minR;
  float acppIr  = maxI - minI;
  double varR = (sRR - sR * sR / n) / n;
  double varI = (sII - sI * sI / n) / n;
  float rmsRed = (varR > 0.0) ? (float)sqrt(varR) : 0.0f;
  float rmsIr  = (varI > 0.0) ? (float)sqrt(varI) : 0.0f;
  if (dcRed <= 0.0f || dcIr <= 0.0f || acppIr <= 0.0f || rmsIr <= 0.0f) {
    spSkipBeat("AC o DC nulos"); return;
  }

  // Razón de razones
  float rPp  = (acppRed / dcRed) / (acppIr / dcIr);
  float rRms = (rmsRed / dcRed) / (rmsIr / dcIr);
  // Pendiente de la regresión AC_RED = m * AC_IR dentro del latido:
  // m = cov(RED, IR) / var(IR). El ruido del canal rojo no está
  // correlacionado con el IR, por lo que se promedia en lugar de inflar
  // la amplitud (el pico a pico y el RMS sí se inflan con el ruido).
  double covRI = (sRI - sR * sI / n) / n;
  float  rReg  = (varI > 0.0) ? (float)(covRI / varI) * (dcIr / dcRed) : 0.0f;
  if (rReg <= 0.0f) { spSkipBeat("R <= 0: RED e IR no correlacionados"); return; }

  // Cambio del DC dentro del latido (primera vs. última muestra)
  int kf = (int)(i0 % SP_BUF_SIZE), kl = (int)(i1 % SP_BUF_SIZE);
  float chRed = fabsf(spDcRed[kl] - spDcRed[kf]) / dcRed;
  float chIr  = fabsf(spDcIr[kl]  - spDcIr[kf])  / dcIr;

  spBeats++;
  spLastIntervalMs = (float)((tPeak - tPrev) * 1000.0 / measuredFs);
  spLastAcppRed = acppRed;  spLastAcppIr = acppIr;
  spLastRmsRed  = rmsRed;   spLastRmsIr  = rmsIr;
  spLastDcRed   = dcRed;    spLastDcIr   = dcIr;
  spLastRpp     = rPp;      spLastRrms   = rRms;
  spLastRreg    = rReg;
  spLastRdiff   = (rPp > 0.0f) ? fabsf(rPp - rRms) / rPp : 1.0f;   // verificación cruzada R_pp vs R_rms
  spLastPiRed   = acppRed / dcRed * 100.0f;
  spLastPiIr    = acppIr  / dcIr  * 100.0f;
  spLastCorr    = correlationFromSums((double)n, sR, sI, sRR, sII, sRI);
  spLastDcChange = (chRed > chIr) ? chRed : chIr;
  spLastQualityOk = (spLastPiIr >= SP_MIN_PI_IR) && (acppRed >= SP_MIN_ACPP_RED) &&
                    (spLastCorr >= SP_MIN_CORR) && (spLastRdiff <= SP_MAX_R_DIFF) &&
                    (spLastDcChange <= SP_MAX_DC_CHANGE);

  spR[spRPos] = rReg;                        // estimador principal
  spRPos = (spRPos + 1) % SP_R_HISTORY;
  if (spRCount < SP_R_HISTORY) spRCount++;

  rtOnBeat();
  spPrintBeatLine();
}

void printSpo2Configuration() {
  Serial.println("  Calculo de R por latido (Etapa 6.1, sin conversion a SpO2):");
  Serial.println("    R = (AC_RED/DC_RED) / (AC_IR/DC_IR) por latido");
  Serial.println("    Estimador principal: pendiente de regresion AC_RED vs AC_IR (insensible al ruido RED)");
  Serial.println("    Comparacion: R pico a pico y R RMS");
  Serial.printf ("    Solo intervalos aceptados | mediana del R de regresion de los ultimos %d latidos\n", SP_R_HISTORY);
  Serial.printf ("    Calidad (informativa): PI IR >= %.2f %%, ACpp RED >= %.0f, corr >= %.2f,\n",
                 SP_MIN_PI_IR, SP_MIN_ACPP_RED, SP_MIN_CORR);
  Serial.printf ("      |R_pp - R_rms| <= %.0f %%, cambio de DC <= %.0f %%\n\n",
                 SP_MAX_R_DIFF * 100.0f, SP_MAX_DC_CHANGE * 100.0f);
}

void printSpo2Second(uint32_t nowMs) {
  Serial.printf("== t=%lus | BPM: %s", (unsigned long)(nowMs / 1000UL), hrStateText(hrState));
  if (hrState == HR_VALID) Serial.printf(" %.1f", hrStableBpm());
  Serial.printf(" | latidos con R %lu (sin R %lu)", (unsigned long)spBeats, (unsigned long)spSkipped);
  if (spRCount > 0) Serial.printf(" | mediana R reg(%d) %.3f | CV R reg %.1f %%", spRCount, spMedianR(), spCvR() * 100.0f);
  Serial.println();
}

// -----------------------------------------------------------------
//  Prueba guiada de R (Etapa 6.1)
// -----------------------------------------------------------------
void rtReset() {
  rtBaseBeats = 0; rtRemBeats = 0; rtStillSat = 0;
  rtN = 0; rtDcRedSum = 0.0; rtDcIrSum = 0.0; rtQualityOk = 0;
}

// Llamada por spo2OnBeat() en cada latido con R calculado
void rtOnBeat() {
  if (rtPhase == RT_BASE_MEASURE) rtBaseBeats++;
  if (rtPhase == RT_REMOVE_MEASURE) rtRemBeats++;
  if (rtPhase != RT_STILL || hrState != HR_VALID || rtN >= RT_MAX_BEATS) return;
  rtR[rtN]     = spLastRreg;
  rtRpp[rtN]   = spLastRpp;
  rtRdiff[rtN] = spLastRdiff;
  rtPiIr[rtN]  = spLastPiIr;
  rtAcRed[rtN] = spLastAcppRed;
  rtCorr[rtN]  = spLastCorr;
  rtDcRedSum  += spLastDcRed;
  rtDcIrSum   += spLastDcIr;
  if (spLastQualityOk) rtQualityOk++;
  rtN++;
}

void rtOnSample(uint32_t red, uint32_t ir) {
  if (rtPhase == RT_STILL && (red >= SATURATION_LEVEL || ir >= SATURATION_LEVEL)) rtStillSat++;
}

void startRTest(uint32_t nowMs) {
  if (!sensorReady) {
    Serial.println(">> El sensor no esta listo; no se puede iniciar la prueba.");
    return;
  }
  rtReset();
  rtModeBefore   = outputMode;
  outputMode     = 'o';
  rtStartMs      = nowMs;
  rtStartSamples = sampleIndex;
  rtStartLost    = lostSamples;
  rtPhase        = RT_BASE_WAIT;
  rtPhaseStartMs = nowMs;

  Serial.println("\n================================================");
  Serial.println(" PRUEBA GUIADA DE R POR LATIDO (aprox. 56 s)");
  Serial.println(" R es una medicion experimental. Aun NO se calcula SpO2.");
  Serial.println("================================================");
  Serial.println(">> FASE 1: RETIRE el dedo y aleje la mano del sensor (> 10 cm).");
  Serial.println("   (3 s de espera + 5 s de medicion)");
}

void finishRTest(uint32_t nowMs) {
  uint32_t testSamples = sampleIndex - rtStartSamples;
  uint32_t testLost    = lostSamples - rtStartLost;
  float    testRate    = testSamples * 1000.0f / (float)(nowMs - rtStartMs);

  float medR    = spMedianOf(rtR, rtN);
  float cvR     = spCvOf(rtR, rtN);
  float medRpp  = spMedianOf(rtRpp, rtN);
  float minR = 0.0f, maxR = 0.0f;
  if (rtN > 0) {
    minR = rtR[0]; maxR = rtR[0];
    for (int i = 1; i < rtN; i++) { if (rtR[i] < minR) minR = rtR[i]; if (rtR[i] > maxR) maxR = rtR[i]; }
  }
  float medDiff = spMedianOf(rtRdiff, rtN);
  float medPi   = spMedianOf(rtPiIr, rtN);
  float medAcR  = spMedianOf(rtAcRed, rtN);
  float medCorr = spMedianOf(rtCorr, rtN);
  float dcRed   = (rtN > 0) ? (float)(rtDcRedSum / rtN) : 0.0f;
  float dcIr    = (rtN > 0) ? (float)(rtDcIrSum / rtN) : 0.0f;

  bool ok1  = (rtBaseBeats == 0);
  bool ok2  = (rtN >= RT_MIN_BEATS);
  bool ok3  = (rtN >= 2) && (cvR <= RT_MAX_CV_R);
  bool ok4  = (rtN > 0) && (medR >= SP_R_MIN_USEFUL) && (medR <= SP_R_MAX_USEFUL);
  bool ok5  = (rtN > 0) && (medDiff <= SP_MAX_R_DIFF);
  bool ok6  = (rtN > 0) && (medPi >= SP_MIN_PI_IR) && (medAcR >= SP_MIN_ACPP_RED);
  bool ok7  = (rtN > 0) && (medCorr >= SP_MIN_CORR);
  bool ok8  = (rtRemBeats == 0);
  bool ok9  = (testLost == 0);
  bool ok10 = (rtStillSat == 0);
  bool ok11 = (testRate >= RATE_MIN && testRate <= RATE_MAX);

  Serial.println("\n================================================");
  Serial.println(" RESULTADOS DE LA PRUEBA DE R (medicion experimental, sin SpO2)");
  Serial.println("================================================");
  Serial.printf ("  Sin dedo      : latidos con R %lu\n", (unsigned long)rtBaseBeats);
  Serial.printf ("  Dedo quieto   : %d latidos con R (con BPM VALIDO) | %d cumplen la calidad preliminar\n",
                 rtN, rtQualityOk);
  if (rtN > 0) {
    Serial.printf("                  R de regresion (principal): mediana %.3f | min %.3f | max %.3f | CV %.1f %%\n",
                  medR, minR, maxR, cvR * 100.0f);
    Serial.printf("                  R pico a pico mediana %.3f (comparacion) | |R_pp - R_rms|/R_pp mediana %.1f %%\n",
                  medRpp, medDiff * 100.0f);
    Serial.printf("                  PI IR mediana %.2f %% | ACpp RED mediana %.0f\n", medPi, medAcR);
    Serial.printf("                  correlacion RED-IR mediana %.2f | DC medio RED %.0f IR %.0f\n",
                  medCorr, dcRed, dcIr);
    if (dcRed > dcIr) {
      Serial.println("                  AVISO: DC RED > DC IR. Con corrientes iguales se espera IR > RED;");
      Serial.println("                  podria indicar un modulo con los LED intercambiados.");
    }
    if (medR < 0.40f || medR > 0.80f) {
      Serial.println("                  NOTA: mediana de R de regresion fuera de 0.40-0.80 (rango habitual en reposo).");
    }
  }
  Serial.printf ("  Retiro        : latidos con R tras retirar %lu\n", (unsigned long)rtRemBeats);
  Serial.printf ("  Adquisicion   : %.1f muestras/s | perdidas %lu | saturadas %lu\n",
                 testRate, (unsigned long)testLost, (unsigned long)rtStillSat);
  Serial.println("------------------------------------------------");
  printTestResult("1. Sin dedo: ningun latido con R", ok1);
  printTestResult("2. >= 15 latidos con R en 30 s con el dedo quieto", ok2);
  printTestResult("3. R estable: CV <= 10 %", ok3);
  printTestResult("4. Mediana de R de regresion en 0.34-1.15 (zona util de la curva de Maxim)", ok4);
  printTestResult("5. R pico a pico y R RMS concuerdan (diferencia mediana <= 15 %)", ok5);
  printTestResult("6. Perfusion suficiente (PI IR >= 0.2 %, ACpp RED >= 50)", ok6);
  printTestResult("7. Correlacion RED-IR mediana >= 0.80", ok7);
  printTestResult("8. Ningun R tras retirar el dedo", ok8);
  printTestResult("9. Sin muestras perdidas", ok9);
  printTestResult("10. Sin saturacion", ok10);
  printTestResult("11. Tasa de muestreo entre 95 y 105 muestras/s", ok11);
  Serial.println("------------------------------------------------");
  if (ok1 && ok2 && ok3 && ok4 && ok5 && ok6 && ok7 && ok8 && ok9 && ok10 && ok11) {
    Serial.println(" RESULTADO: PRUEBA DE R SUPERADA (Etapa 6.1)");
    Serial.printf (" Mediana de R de regresion: %.3f  (medicion experimental; aun sin conversion a SpO2)\n", medR);
  } else {
    Serial.println(" RESULTADO: PRUEBA DE R NO SUPERADA. Sugerencias:");
    if (!ok1 || !ok8) Serial.println("  - Aleje por completo la mano durante las fases sin dedo.");
    if (!ok2) Serial.println("  - Pocos latidos: verifique que el BPM llegue a BPM VALIDO (comando 'b').");
    if (!ok3 || !ok5 || !ok7) Serial.println("  - R inestable: mantenga dedo y cable quietos, sin presionar.");
    if (!ok4) Serial.println("  - R fuera de rango: revise el orden RED/IR y la colocacion del dedo.");
    if (!ok6) Serial.println("  - Perfusion baja: caliente la mano o reposicione el dedo.");
    if (!ok9 || !ok11) Serial.println("  - Problema de adquisicion: revise el diagnostico ('d').");
    if (!ok10) Serial.println("  - Saturacion: presione menos o reduzca la corriente con '-'.");
  }
  Serial.println("================================================\n");

  rtPhase    = RT_IDLE;
  outputMode = rtModeBefore;
}

void updateRTest(uint32_t nowMs) {
  if (rtPhase == RT_IDLE) return;
  uint32_t elapsed = nowMs - rtPhaseStartMs;

  if (rtPhase == RT_BASE_WAIT && elapsed >= RT_BASE_WAIT_MS) {
    rtPhase = RT_BASE_MEASURE;
    rtPhaseStartMs = nowMs;
    Serial.println(">> Midiendo SIN dedo (5 s)...");
  } else if (rtPhase == RT_BASE_MEASURE && elapsed >= RT_BASE_MEASURE_MS) {
    rtPhase = RT_PLACE;
    rtPhaseStartMs = nowMs;
    Serial.println(">> FASE 2: COLOQUE la yema del dedo sobre el sensor, SIN presionar.");
    Serial.println("   Apoye la mano en la mesa y no se mueva. (10 s de estabilizacion)");
  } else if (rtPhase == RT_PLACE && elapsed >= RT_PLACE_MS) {
    rtPhase = RT_STILL;
    rtPhaseStartMs = nowMs;
    Serial.println(">> FASE 3: MANTENGA el dedo QUIETO. Midiendo 30 s...");
  } else if (rtPhase == RT_STILL && elapsed >= RT_STILL_MS) {
    rtPhase = RT_REMOVE_WAIT;
    rtPhaseStartMs = nowMs;
    Serial.println(">> FASE 4: RETIRE el dedo y aleje la mano (> 10 cm).");
    Serial.println("   (3 s de espera + 5 s de medicion)");
  } else if (rtPhase == RT_REMOVE_WAIT && elapsed >= RT_REMOVE_WAIT_MS) {
    rtPhase = RT_REMOVE_MEASURE;
    rtPhaseStartMs = nowMs;
    Serial.println(">> Midiendo SIN dedo nuevamente (5 s)...");
  } else if (rtPhase == RT_REMOVE_MEASURE && elapsed >= RT_REMOVE_MEASURE_MS) {
    finishRTest(nowMs);
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
  spo2StoreSample(red, ir);            // búfer RED/IR para R (Etapa 6.1)
  updateHeartRate(red, ir);            // detección de pulsos y BPM (Etapa 5)
  bpmTestOnSample(millis(), red, ir);
  rtOnSample(red, ir);

  if (outputMode == 'k') {
    // Serial Plotter: PPG IR, umbral adaptativo y marca de pico candidato
    Serial.printf("PPG_IR:%.1f,Umbral:%.1f,Pico:%.1f\n",
                  -lastAc[CH_IR], hrThreshold, hrPeakMarker);
  } else if (outputMode == 'p') {
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
  Serial.println("  b -> monitor de BPM: una linea por pulso + resumen por segundo");
  Serial.println("  k -> Serial Plotter: PPG IR, umbral adaptativo y picos");
  Serial.println("  v -> prueba guiada de BPM (aprox. 46 s)");
  Serial.println("  o -> monitor de R por latido (Etapa 6.1, sin SpO2)");
  Serial.println("  c -> CSV con una fila por latido (R, AC, DC, PI, correlacion)");
  Serial.println("  u -> prueba guiada de R (aprox. 56 s)");
  Serial.println("  d -> diagnostico PPG por segundo (DC, AC p-p, AC rms, AC/DC)");
  Serial.println("  p -> procesamiento: crudo, DC y AC de RED e IR (1 de cada 10)");
  Serial.println("  g -> Serial Plotter: PPG de IR y RED (todas las muestras)");
  Serial.println("  x -> CSV: n,RED,IR,RED_DC,IR_DC,RED_AC,IR_AC");
  Serial.println("  t -> prueba guiada de la senal PPG, Etapa 4.1 (aprox. 31 s)");
  Serial.println("  r -> reiniciar filtros y detector de pulsos");
  Serial.println("  + -> aumentar corriente de LED (1 mA)");
  Serial.println("  - -> disminuir corriente de LED (1 mA)");
  Serial.println("  i -> configuracion, registros y coeficientes");
  Serial.println("  h -> ayuda\n");
}

bool anyTestRunning() {
  return (testPhase != TEST_IDLE) || (btPhase != BT_IDLE) || (rtPhase != RT_IDLE);
}

void setOutputMode(char mode) {
  if (testPhase != TEST_IDLE) {
    Serial.println(">> Prueba en curso: el modo se aplicara al terminar.");
    modeBeforeTest = mode;
    return;
  }
  if (btPhase != BT_IDLE) {
    Serial.println(">> Prueba en curso: el modo se aplicara al terminar.");
    btModeBefore = mode;
    return;
  }
  if (rtPhase != RT_IDLE) {
    Serial.println(">> Prueba en curso: el modo se aplicara al terminar.");
    rtModeBefore = mode;
    return;
  }
  outputMode = mode;
  if (mode == 'b') { Serial.println(">> Modo: monitor de BPM"); hrPrevState = -1; }
  if (mode == 'o') Serial.println(">> Modo: monitor de R por latido (sin SpO2)");
  if (mode == 'c') Serial.println("latido,t_ms,int_ms,acpp_red,acpp_ir,dc_red,dc_ir,rms_red,rms_ir,r_reg,r_pp,r_rms,pi_ir,pi_red,corr,ddc_pct,estado_bpm");
  if (mode == 'd') Serial.println(">> Modo: diagnostico por segundo");
  if (mode == 'p') Serial.println(">> Modo: procesamiento (1 de cada 10 muestras)");
  if (mode == 'x') Serial.println("n,RED,IR,RED_DC,IR_DC,RED_AC,IR_AC");
  // En modos 'g' y 'k' no se imprime texto extra para no alterar el Serial Plotter
}

void handleSerialCommands(uint32_t nowMs) {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r' || c == '\n' || c == ' ') continue;

    if (c == 'b' || c == 'B') {
      setOutputMode('b');
    } else if (c == 'k' || c == 'K') {
      setOutputMode('k');
    } else if (c == 'v' || c == 'V') {
      if (!anyTestRunning()) startBpmTest(nowMs);
      else Serial.println(">> Ya hay una prueba en curso.");
    } else if (c == 'o' || c == 'O') {
      setOutputMode('o');
    } else if (c == 'c' || c == 'C') {
      setOutputMode('c');
    } else if (c == 'u' || c == 'U') {
      if (!anyTestRunning()) startRTest(nowMs);
      else Serial.println(">> Ya hay una prueba en curso.");
    } else if (c == 'd' || c == 'D') {
      setOutputMode('d');
    } else if (c == 'p' || c == 'P') {
      setOutputMode('p');
    } else if (c == 'g' || c == 'G') {
      setOutputMode('g');
    } else if (c == 'x' || c == 'X') {
      setOutputMode('x');
    } else if (c == 't' || c == 'T') {
      if (!anyTestRunning()) startPpgTest(nowMs);
      else Serial.println(">> Ya hay una prueba en curso.");
    } else if (c == 'r' || c == 'R') {
      if (anyTestRunning()) {
        Serial.println(">> No se reinician los filtros durante la prueba.");
      } else {
        filtersPrimed = false;       // se reinician con la siguiente muestra
        manualResets++;              // (el detector de pulsos se reinicia con ellos)
        Serial.println(">> Filtros y detector reiniciados (2 s de estabilizacion).");
      }
    } else if (c == '+') {
      if (anyTestRunning()) Serial.println(">> No se cambia la corriente durante la prueba.");
      else if (ledCurrent + LED_CURRENT_STEP <= LED_CURRENT_MAX) setLedCurrent(ledCurrent + LED_CURRENT_STEP);
      else Serial.println(">> Corriente maxima de esta etapa alcanzada (16 mA).");
    } else if (c == '-') {
      if (anyTestRunning()) Serial.println(">> No se cambia la corriente durante la prueba.");
      else if (ledCurrent >= LED_CURRENT_MIN + LED_CURRENT_STEP) setLedCurrent(ledCurrent - LED_CURRENT_STEP);
      else Serial.println(">> Corriente minima alcanzada (1 mA).");
    } else if (c == 'i' || c == 'I') {
      if (sensorReady) printConfiguration();
      printFilterConfiguration();
      printHeartRateConfiguration();
      printSpo2Configuration();
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
  Serial.println(" ETAPA 6.1 - Calculo de R por latido (RED/IR)");
  Serial.println(" Prototipo academico - NO es dispositivo medico");
  Serial.println(" BPM y R = mediciones experimentales. Aun NO se calcula SpO2");
  Serial.println("================================================");

  computeFilterCoefficients();
  hrComputeConstants();
  hrResetDetector();
  sensorReady = initSensor();
  printFilterConfiguration();
  printHeartRateConfiguration();
  printSpo2Configuration();

  if (sensorReady) {
    Serial.println("Adquisicion iniciada. 'o' = monitor de R, 'u' = prueba guiada de R, 'b' = BPM, 'h' = ayuda.\n");
  } else {
    Serial.println("Se reintentara la inicializacion cada 5 s.\n");
  }
  uint32_t now = millis();
  lastPollMs  = now;
  lastRetryMs = now;
  resetWindow(now);
  resetFsMeasurement(now);
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

  // Pruebas guiadas (máquinas de estados no bloqueantes)
  updatePpgTest(now);
  updateBpmTest(now);
  updateRTest(now);

  // Diagnóstico de cada segundo
  reportWindow(now);
}
