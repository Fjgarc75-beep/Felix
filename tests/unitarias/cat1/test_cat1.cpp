// =====================================================================
//  test_cat1.cpp - Etapa 10, Categoría 1: funciones puras o deterministas
//  Proyecto: Monitor PPG experimental (Maestría en Ingeniería Clínica)
//
//  El firmware NO se modifica: run_tests.sh copia Etapa9.ino (MD5
//  4d70eb1aef89be2d7ab61ec6d39b9f12), lo preprocesa con arduino-cli (el
//  mismo preprocesador del IDE) y este archivo incluye ese .cpp en la
//  misma unidad de compilación para acceder a funciones y variables.
//
//  Cada caso indica la función, el tipo (N normal, L límite, E error) y
//  lo que verifica. Los valores esperados vienen de:
//    - cálculo a mano documentado junto al caso, o
//    - reference/gen_reference_cat1.py (scipy), independiente del firmware.
//
//  Variante CAT1_VARIANTE_DEC0: compila una COPIA preprocesada con
//  SPO2_DECIMALS = 0 (solo en build/, nunca en el firmware) para los casos
//  del modo entero de SpO2.
// =====================================================================
#ifndef FIRMWARE_CPP
#define FIRMWARE_CPP "Etapa9.ino.cpp"
#endif
#include FIRMWARE_CPP
#include "mini_test.h"
#include "ref_cat1.h"
#include <algorithm>

// ---------------------------------------------------------------------
//  Utilidades de prueba
// ---------------------------------------------------------------------
static double UNO[200];                          // escalón unitario (se llena en antesDeCadaCaso)

static void antesDeCadaCaso() {
  computeFilterCoefficients();
  hrComputeConstants();
  measuredFs = 100.0f;
  hrHistCount = 0; hrHistPos = 0; hrHistCv = 0.0f;
  for (int i = 0; i < HR_HISTORY_SIZE; i++) hrHistory[i] = 0.0f;
  spRCount = 0; spRPos = 0;
  for (int ch = 0; ch < 2; ch++) {
    hpZ1[ch] = hpZ2[ch] = lpZ1[ch] = lpZ2[ch] = 0.0f;
    dcEstimate[ch] = 0.0f; lastDc[ch] = 0.0f;
  }
  filtersPrimed = false; settleRemaining = 0;
  testPhase = TEST_IDLE; btPhase = BT_IDLE; rtPhase = RT_IDLE; stPhase = ST_IDLE;
  sensorReady = true; hrState = HR_NO_SIGNAL; oxState = OX_NO_SIGNAL;
  uiPpgCenter = 0.0f; uiPpgSpan = 1000.0f;
  for (int k = 0; k < 200; k++) UNO[k] = 1.0;
}

static void fijarHistorial(const float *v, int n) {
  hrHistCount = n; hrHistPos = n % HR_HISTORY_SIZE;
  for (int i = 0; i < n; i++) hrHistory[i] = v[i];
}

static float corrDeVectores(const std::vector<double> &r, const std::vector<double> &i, int n) {
  double sR = 0, sI = 0, sRR = 0, sII = 0, sRI = 0;
  for (int k = 0; k < n; k++) { sR += r[k]; sI += i[k]; sRR += r[k] * r[k]; sII += i[k] * i[k]; sRI += r[k] * i[k]; }
  return correlationFromSums((double)n, sR, sI, sRR, sII, sRI);
}

// Respuesta en frecuencia de un biquad con los coeficientes del firmware
static double gananciaDb(float b0, float b1, float b2, float a1, float a2, double f, double fs) {
  double w = 2.0 * M_PI * f / fs;
  double nr = b0 + b1 * cos(-w) + b2 * cos(-2 * w), ni = b1 * sin(-w) + b2 * sin(-2 * w);
  double dr = 1 + a1 * cos(-w) + a2 * cos(-2 * w), di = a1 * sin(-w) + a2 * sin(-2 * w);
  return 20.0 * log10(sqrt(nr * nr + ni * ni) / sqrt(dr * dr + di * di));
}

static bool biquadEstable(float a1, float a2) {      // polos de z^2 + a1 z + a2 dentro del círculo unitario
  return fabsf(a2) < 1.0f && fabsf(a1) < 1.0f + a2;
}

// Compara la salida del filtro del firmware con la referencia de scipy
static double errorMaxFiltro(bool pasaAltas, const double *in, const double *ref, int n, int ch) {
  double maxErr = 0, maxRef = 0;
  for (int k = 0; k < n; k++) {
    float y = pasaAltas ? highPass(ch, (float)in[k]) : lowPass(ch, (float)in[k]);
    maxErr = std::max(maxErr, fabs((double)y - ref[k]));
    maxRef = std::max(maxRef, fabs(ref[k]));
  }
  return maxErr / (maxRef > 0 ? maxRef : 1.0);
}


#ifndef CAT1_VARIANTE_DEC0
// =====================================================================
//  01 correlationFromSums
// =====================================================================
TEST_CASE(C1_01_N, "correlationFromSums", "N", "R = 2*I + 3 da 1.0; R = -I da -1.0 (100 muestras)") {
  std::vector<double> i(100), r(100), rn(100);
  for (int k = 0; k < 100; k++) { i[k] = sin(k * 0.3) * 50 + k * 0.1; r[k] = 2 * i[k] + 3; rn[k] = -i[k]; }
  CHECK_NEAR(corrDeVectores(r, i, 100), 1.0, 1e-6);
  CHECK_NEAR(corrDeVectores(rn, i, 100), -1.0, 1e-6);
}
TEST_CASE(C1_01_L, "correlationFromSums", "L", "n = 1 y señal constante (varianza 0) dan 0") {
  CHECK_EQ(correlationFromSums(1.0, 5, 5, 25, 25, 25), 0.0f);
  std::vector<double> c(50, 7.0), i(50);
  for (int k = 0; k < 50; k++) i[k] = k;
  CHECK_EQ(corrDeVectores(c, i, 50), 0.0f);
}
TEST_CASE(C1_01_E, "correlationFromSums", "E", "n = 0, sumas incoherentes (varianza negativa) y n = 1 incoherente dan 0, sin NaN") {
  CHECK_EQ(correlationFromSums(0.0, 0, 0, 0, 0, 0), 0.0f);
  float v = correlationFromSums(10.0, 100, 100, 1, 1, 1);       // varR = 1 - 1000 < 0
  CHECK_EQ(v, 0.0f);
  CHECK(std::isfinite(v));
  // n = 1 con sumas incoherentes (varianzas aparentes > 0): la guarda n < 2 debe actuar
  CHECK_EQ(correlationFromSums(1.0, 1, 1, 5, 5, 5), 0.0f);
}
TEST_CASE(C1_01_L2, "correlationFromSums", "L", "IR constante con roja variable (varR > 0, varI = 0) da 0") {
  std::vector<double> r(50), c(50, 3.0);
  for (int k = 0; k < 50; k++) r[k] = k;
  CHECK_EQ(corrDeVectores(r, c, 50), 0.0f);
}

// =====================================================================
//  02 hrParabolicOffset
// =====================================================================
TEST_CASE(C1_02_N, "hrParabolicOffset", "N", "(1,3,2) = +1/6; (2,3,1) = -1/6; (1,2,1) = 0") {
  CHECK_NEAR(hrParabolicOffset(1, 3, 2), 1.0 / 6.0, 1e-6);
  CHECK_NEAR(hrParabolicOffset(2, 3, 1), -1.0 / 6.0, 1e-6);
  CHECK_NEAR(hrParabolicOffset(1, 2, 1), 0.0, 1e-9);
}
TEST_CASE(C1_02_L, "hrParabolicOffset", "L", "denominador 0 (2,2,2) = 0; (0,1,1.9) se recorta a +0.5") {
  CHECK_EQ(hrParabolicOffset(2, 2, 2), 0.0f);
  CHECK_NEAR(hrParabolicOffset(0, 1, 1.9f), 0.5, 1e-9);
  CHECK_NEAR(hrParabolicOffset(1.9f, 1, 0), -0.5, 1e-9);
}
TEST_CASE(C1_02_E, "hrParabolicOffset", "E", "(1,1,5) no es un máximo: devuelve 0") {
  CHECK_EQ(hrParabolicOffset(1, 1, 5), 0.0f);
}

// =====================================================================
//  03 spMedianOf
// =====================================================================
TEST_CASE(C1_03_N, "spMedianOf", "N", "[3,1,2] = 2; [4,1,3,2] = 2.5; el arreglo de entrada no cambia") {
  float a[3] = {3, 1, 2}, b[4] = {4, 1, 3, 2};
  CHECK_EQ(spMedianOf(a, 3), 2.0f);
  CHECK_EQ(spMedianOf(b, 4), 2.5f);
  CHECK(a[0] == 3 && a[1] == 1 && a[2] == 2);
}
TEST_CASE(C1_03_L, "spMedianOf", "L", "n = 1; n = 64 (1..64) = 32.5; n = 65 usa solo los 64 primeros") {
  float one[1] = {7.5f};
  CHECK_EQ(spMedianOf(one, 1), 7.5f);
  float v[65];
  for (int k = 0; k < 64; k++) v[k] = (float)(64 - k);   // desordenado
  v[64] = 1000.0f;
  CHECK_EQ(spMedianOf(v, 64), 32.5f);
  CHECK_EQ(spMedianOf(v, 65), 32.5f);
}
TEST_CASE(C1_03_E, "spMedianOf", "E", "n = 0 y n negativo devuelven 0") {
  float a[1] = {9};
  CHECK_EQ(spMedianOf(a, 0), 0.0f);
  CHECK_EQ(spMedianOf(a, -3), 0.0f);
}

// =====================================================================
//  04 spCvOf
// =====================================================================
TEST_CASE(C1_04_N, "spCvOf", "N", "[1,2,3]: desviación muestral 1, media 2, CV 0.5") {
  float a[3] = {1, 2, 3};
  CHECK_NEAR(spCvOf(a, 3), 0.5, 1e-6);
}
TEST_CASE(C1_04_L, "spCvOf", "L", "n = 2 [2,4] = 0.4714; valores iguales = 0") {
  float a[2] = {2, 4}, b[3] = {5, 5, 5};
  CHECK_NEAR(spCvOf(a, 2), sqrt(2.0) / 3.0, 1e-6);
  CHECK_EQ(spCvOf(b, 3), 0.0f);
}
TEST_CASE(C1_04_E, "spCvOf", "E", "n < 2 y media <= 0 devuelven 0 (subCvPP devuelve 99 en ese caso: diferencia documentada)") {
  float one[1] = {3}, neg[3] = {-1, -2, -3};
  CHECK_EQ(spCvOf(one, 1), 0.0f);
  CHECK_EQ(spCvOf(neg, 3), 0.0f);
}

// =====================================================================
//  05 oxSpo2FromR
// =====================================================================
TEST_CASE(C1_05_N, "oxSpo2FromR", "N", "R = 0.428 -> 99.58 %; R = 0.5 -> 98.76 % (referencia Python)") {
  CHECK_NEAR(oxSpo2FromR((float)REF_R_VALUES[0]), REF_SPO2_VALUES[0], 0.01);
  CHECK_NEAR(oxSpo2FromR((float)REF_R_VALUES[1]), REF_SPO2_VALUES[1], 0.01);
}
TEST_CASE(C1_05_L, "oxSpo2FromR", "L", "vértice 0.337 -> 99.96; 0.34 -> 99.96; 1.15 -> 70.16") {
  CHECK_NEAR(oxSpo2FromR((float)REF_R_VALUES[2]), REF_SPO2_VALUES[2], 0.01);
  CHECK_NEAR(oxSpo2FromR((float)REF_R_VALUES[3]), REF_SPO2_VALUES[3], 0.01);
  CHECK_NEAR(oxSpo2FromR((float)REF_R_VALUES[4]), REF_SPO2_VALUES[4], 0.01);
  CHECK(oxSpo2FromR(0.34f) <= 100.0f);
}
TEST_CASE(C1_05_E, "oxSpo2FromR", "E", "R = 0 devuelve c = 94.845 (fuera del rango útil; los estados lo filtran)") {
  CHECK_NEAR(oxSpo2FromR(0.0f), REF_SPO2_VALUES[5], 0.001);
}

// =====================================================================
//  06-11 Textos
// =====================================================================
TEST_CASE(C1_06_N, "hrStateText", "N", "los 3 estados del BPM") {
  CHECK_STR(hrStateText(HR_NO_SIGNAL), "SIN SENAL");
  CHECK_STR(hrStateText(HR_STABILIZING), "ESTABILIZANDO");
  CHECK_STR(hrStateText(HR_VALID), "BPM VALIDO");
}
TEST_CASE(C1_06_E, "hrStateText", "E", "estados fuera de rango (99, -1) -> SIN SENAL") {
  CHECK_STR(hrStateText(99), "SIN SENAL");
  CHECK_STR(hrStateText(-1), "SIN SENAL");
}
TEST_CASE(C1_07_N, "oxStateText", "N", "los 5 estados de SpO2, textos distintos") {
  CHECK_STR(oxStateText(OX_VALID), "SpO2 VALIDA (estimacion experimental)");
  CHECK_STR(oxStateText(OX_OUT_OF_RANGE), "FUERA DE RANGO");
  CHECK_STR(oxStateText(OX_LOW_QUALITY), "CALIDAD INSUFICIENTE");
  CHECK_STR(oxStateText(OX_STABILIZING), "ESTABILIZANDO");
  CHECK_STR(oxStateText(OX_NO_SIGNAL), "SIN SENAL");
}
TEST_CASE(C1_07_E, "oxStateText", "E", "fuera de rango (-1, 7) -> SIN SENAL") {
  CHECK_STR(oxStateText(-1), "SIN SENAL");
  CHECK_STR(oxStateText(7), "SIN SENAL");
}
TEST_CASE(C1_08_N, "calidadTexto", "N", "las 5 categorías confirmadas") {
  CHECK_STR(calidadTexto(CAL_BUENA), "BUENA");
  CHECK_STR(calidadTexto(CAL_REGULAR), "REGULAR");
  CHECK_STR(calidadTexto(CAL_MALA), "MALA");
  CHECK_STR(calidadTexto(CAL_SIN_DEDO), "SIN DEDO");
  CHECK_STR(calidadTexto(CAL_BUSCANDO), "BUSCANDO");
}
TEST_CASE(C1_08_L, "calidadTexto", "L", "todas caben en el campo de 8 caracteres") {
  for (int c = 0; c < NUM_CAL; c++) CHECK(strlen(calidadTexto(c)) <= (size_t)UI_Q_CHARS);
}
TEST_CASE(C1_08_E, "calidadTexto", "E", "fuera de rango (-1, 9) -> BUSCANDO") {
  CHECK_STR(calidadTexto(-1), "BUSCANDO");
  CHECK_STR(calidadTexto(9), "BUSCANDO");
}
TEST_CASE(C1_09_N, "calidadColor", "N", "verde, amarillo, naranja, gris claro, gris claro") {
  CHECK_EQ(calidadColor(CAL_BUENA), (uint16_t)0x07E0);
  CHECK_EQ(calidadColor(CAL_REGULAR), (uint16_t)0xFFE0);
  CHECK_EQ(calidadColor(CAL_MALA), (uint16_t)0xFC00);
  CHECK_EQ(calidadColor(CAL_SIN_DEDO), (uint16_t)0xC618);
  CHECK_EQ(calidadColor(CAL_BUSCANDO), (uint16_t)0xC618);
}
TEST_CASE(C1_09_E, "calidadColor", "E", "fuera de rango -> gris claro") {
  CHECK_EQ(calidadColor(-1), (uint16_t)0xC618);
  CHECK_EQ(calidadColor(9), (uint16_t)0xC618);
}
TEST_CASE(C1_10_N, "modoTexto", "N", "los 3 textos del pie") {
  CHECK_STR(modoTexto(MODO_EXPERIMENTAL), "ESTIM. EXPERIMENTAL");
  CHECK_STR(modoTexto(MODO_SIN_SENSOR), "SENSOR NO DETECTADO");
  CHECK_STR(modoTexto(MODO_SIN_DATOS), "SENSOR NO RESPONDE");
}
TEST_CASE(C1_10_L, "modoTexto", "L", "todos caben en el campo de 21 caracteres") {
  int modos[3] = {MODO_EXPERIMENTAL, MODO_SIN_SENSOR, MODO_SIN_DATOS};
  for (int m : modos) CHECK(strlen(modoTexto(m)) <= (size_t)UI_PIE_CHARS);
}
TEST_CASE(C1_10_E, "modoTexto", "E", "modo desconocido (0, 9) -> ESTIM. EXPERIMENTAL") {
  CHECK_STR(modoTexto(0), "ESTIM. EXPERIMENTAL");
  CHECK_STR(modoTexto(9), "ESTIM. EXPERIMENTAL");
}
TEST_CASE(C1_11_N, "campoNombre", "N", "los 4 campos, 7 caracteres cada uno") {
  CHECK_STR(campoNombre(UI_F_BPM), "BPM    ");
  CHECK_STR(campoNombre(UI_F_SPO2), "SpO2   ");
  CHECK_STR(campoNombre(UI_F_CAL), "Calidad");
  CHECK_STR(campoNombre(UI_F_PIE), "Pie    ");
  for (int f = 0; f < 4; f++) CHECK_EQ(strlen(campoNombre(f)), (size_t)7);
}
TEST_CASE(C1_11_E, "campoNombre", "E", "campo fuera de rango (7) -> \"Pie    \"") {
  CHECK_STR(campoNombre(7), "Pie    ");
}

// =====================================================================
//  12 uiTextoCentrado
// =====================================================================
TEST_CASE(C1_12_N, "uiTextoCentrado", "N", "(\"BUENA\", 8) -> \" BUENA  \"") {
  char b[16];
  uiTextoCentrado(b, 8, "BUENA");
  CHECK_STR(b, " BUENA  ");
}
TEST_CASE(C1_12_L, "uiTextoCentrado", "L", "texto de 8 queda igual; vacío = 8 espacios; ancho 21") {
  char b[32];
  uiTextoCentrado(b, 8, "SIN DEDO"); CHECK_STR(b, "SIN DEDO");
  uiTextoCentrado(b, 8, "");         CHECK_STR(b, "        ");
  uiTextoCentrado(b, 21, "DATOS");   CHECK_STR(b, "        DATOS        ");
  CHECK_EQ(strlen(b), (size_t)21);
}
TEST_CASE(C1_12_E, "uiTextoCentrado", "E", "texto más largo que el ancho: truncado, sin escribir después de out[ancho]") {
  char b[16];
  memset(b, '#', sizeof(b));
  uiTextoCentrado(b, 8, "ABCDEFGHIJ");
  CHECK_STR(b, "ABCDEFGH");
  CHECK_EQ(b[9], '#');                     // byte centinela intacto
}

// =====================================================================
//  13 formatSpo2Key (modo con 1 decimal; el modo entero está en la variante)
// =====================================================================
TEST_CASE(C1_13_N, "formatSpo2Key", "N", "985 -> \" 98.5\"; 1000 -> \"100.0\"") {
  char b[12];
  formatSpo2Key(b, sizeof(b), 985);  CHECK_STR(b, " 98.5");
  formatSpo2Key(b, sizeof(b), 1000); CHECK_STR(b, "100.0");
}
TEST_CASE(C1_13_L, "formatSpo2Key", "L", "0 -> \"  0.0\"; 5 -> \"  0.5\"; todas de 5 caracteres") {
  char b[12];
  formatSpo2Key(b, sizeof(b), 0); CHECK_STR(b, "  0.0");
  formatSpo2Key(b, sizeof(b), 5); CHECK_STR(b, "  0.5");
  CHECK_EQ(strlen(b), (size_t)5);
}
TEST_CASE(C1_13_E, "formatSpo2Key", "E", "-1 -> \" --.-\"; búfer de 6 y de 4 bytes sin desborde (byte centinela)") {
  char b[12];
  formatSpo2Key(b, sizeof(b), -1); CHECK_STR(b, " --.-");
  memset(b, '#', sizeof(b));
  formatSpo2Key(b, 6, 1000); CHECK_STR(b, "100.0"); CHECK_EQ(b[6], '#');
  memset(b, '#', sizeof(b));
  formatSpo2Key(b, 4, 1000); CHECK_STR(b, "100"); CHECK_EQ(b[4], '#');
}

// =====================================================================
//  14-17 Coordenadas y textos de diagnóstico
// =====================================================================
TEST_CASE(C1_14_N, "uiSpo2FieldX", "N", "con 1 decimal: x = 4 y el campo de 5 caracteres termina antes del %") {
  CHECK_EQ(uiSpo2FieldX(), 4);
  CHECK(uiSpo2FieldX() + 5 * 6 * UI_SP_SIZE <= uiPctX());
}
TEST_CASE(C1_15_N, "uiPctX", "N", "el % está en x = 98") {
  CHECK_EQ(uiPctX(), 98);
}
TEST_CASE(C1_16_N, "resetReasonText", "N", "encendido, reset externo, software, panic") {
  CHECK_STR(resetReasonText(ESP_RST_POWERON), "encendido");
  CHECK_STR(resetReasonText(ESP_RST_EXT), "reset externo");
  CHECK_STR(resetReasonText(ESP_RST_SW), "reinicio por software");
  CHECK_STR(resetReasonText(ESP_RST_PANIC), "error del programa (panic)");
}
TEST_CASE(C1_16_L, "resetReasonText", "L", "los 3 watchdog y el brownout") {
  CHECK_STR(resetReasonText(ESP_RST_INT_WDT), "watchdog");
  CHECK_STR(resetReasonText(ESP_RST_TASK_WDT), "watchdog");
  CHECK_STR(resetReasonText(ESP_RST_WDT), "watchdog");
  CHECK_STR(resetReasonText(ESP_RST_BROWNOUT), "CAIDA DE VOLTAJE (brownout)");
}
TEST_CASE(C1_16_E, "resetReasonText", "E", "motivo desconocido (UNKNOWN, 99) -> \"otro\"") {
  CHECK_STR(resetReasonText(ESP_RST_UNKNOWN), "otro");
  CHECK_STR(resetReasonText(99), "otro");
}
TEST_CASE(C1_17_N, "arranqueTipoTexto", "N", "arranque, reintento, recuperacion P2") {
  CHECK_STR(arranqueTipoTexto(ARR_ARRANQUE), "arranque");
  CHECK_STR(arranqueTipoTexto(ARR_REINTENTO), "reintento");
  CHECK_STR(arranqueTipoTexto(ARR_RECUPERACION), "recuperacion P2");
}
TEST_CASE(C1_17_E, "arranqueTipoTexto", "E", "tipo desconocido (5) -> arranque") {
  CHECK_STR(arranqueTipoTexto(5), "arranque");
}

// =====================================================================
//  18 isLargeStep
// =====================================================================
TEST_CASE(C1_18_N, "isLargeStep", "N", "DC 100000: x 110001 y 89999 son escalón; 109999 no") {
  dcEstimate[CH_IR] = 100000.0f;
  CHECK(isLargeStep(CH_IR, 110001.0f));
  CHECK(isLargeStep(CH_IR, 89999.0f));
  CHECK(!isLargeStep(CH_IR, 109999.0f));
}
TEST_CASE(C1_18_L, "isLargeStep", "L", "x = 110000 exacto no es escalón (estricto); DC 20000 usa el mínimo de 5000") {
  dcEstimate[CH_IR] = 100000.0f;
  CHECK(!isLargeStep(CH_IR, 110000.0f));
  dcEstimate[CH_RED] = 20000.0f;
  CHECK(isLargeStep(CH_RED, 25001.0f));
  CHECK(!isLargeStep(CH_RED, 25000.0f));
}
TEST_CASE(C1_18_E, "isLargeStep", "E", "DC 0: 4999 no, 5001 sí; los canales son independientes") {
  dcEstimate[CH_RED] = 0.0f; dcEstimate[CH_IR] = 100000.0f;
  CHECK(!isLargeStep(CH_RED, 4999.0f));
  CHECK(isLargeStep(CH_RED, 5001.0f));
  CHECK(!isLargeStep(CH_IR, 105000.0f));
}

// =====================================================================
//  19 filtersSettled
// =====================================================================
TEST_CASE(C1_19_N, "filtersSettled", "N", "(primed, 0) -> true") {
  filtersPrimed = true; settleRemaining = 0;
  CHECK(filtersSettled());
}
TEST_CASE(C1_19_L, "filtersSettled", "L", "(primed, 1) -> false") {
  filtersPrimed = true; settleRemaining = 1;
  CHECK(!filtersSettled());
}
TEST_CASE(C1_19_E, "filtersSettled", "E", "(no primed, 0) -> false") {
  filtersPrimed = false; settleRemaining = 0;
  CHECK(!filtersSettled());
}

// =====================================================================
//  20 computeFilterCoefficients
// =====================================================================
TEST_CASE(C1_20_N, "computeFilterCoefficients", "N", "coeficientes = scipy.butter (fs 100); -3.01 dB en fc; ganancia DC 0 y 1") {
  computeFilterCoefficients();
  const float hp[5] = {hpB0, hpB1, hpB2, hpA1, hpA2}, lp[5] = {lpB0, lpB1, lpB2, lpA1, lpA2};
  const double hr[5] = {REF_HP_B[0], REF_HP_B[1], REF_HP_B[2], REF_HP_A[1], REF_HP_A[2]};
  const double lr[5] = {REF_LP_B[0], REF_LP_B[1], REF_LP_B[2], REF_LP_A[1], REF_LP_A[2]};
  for (int k = 0; k < 5; k++) { CHECK_NEAR(hp[k], hr[k], 1e-5); CHECK_NEAR(lp[k], lr[k], 1e-5); }
  CHECK_NEAR(gananciaDb(hpB0, hpB1, hpB2, hpA1, hpA2, HP_CUTOFF_HZ, FS_NOMINAL), REF_HP_GAIN_DB_FC, 0.1);
  CHECK_NEAR(gananciaDb(lpB0, lpB1, lpB2, lpA1, lpA2, LP_CUTOFF_HZ, FS_NOMINAL), REF_LP_GAIN_DB_FC, 0.1);
  CHECK_NEAR(hpB0 + hpB1 + hpB2, 0.0, 1e-6);                                  // |H(0)| pasa-altas = 0
  CHECK_NEAR((lpB0 + lpB1 + lpB2) / (1 + lpA1 + lpA2), 1.0, 1e-4);            // |H(0)| pasa-bajas = 1
}
TEST_CASE(C1_20_L, "computeFilterCoefficients", "L", "alfas del DC: 0.009950 (tau 1 s) y 0.095163 (tau 0.1 s)") {
  computeFilterCoefficients();
  CHECK_NEAR(dcAlpha, REF_DC_ALPHA, 1e-6);
  CHECK_NEAR(dcAlphaFast, REF_DC_ALPHA_FAST, 1e-6);
}
TEST_CASE(C1_20_E, "computeFilterCoefficients", "E", "ambos biquads estables (polos dentro del círculo unitario)") {
  computeFilterCoefficients();
  CHECK(biquadEstable(hpA1, hpA2));
  CHECK(biquadEstable(lpA1, lpA2));
}

// =====================================================================
//  21 hrComputeConstants
// =====================================================================
TEST_CASE(C1_21_N, "hrComputeConstants", "N", "hrEnvDecay = exp(-1/200) = 0.995012") {
  hrEnvDecay = 0.0f;
  hrComputeConstants();
  CHECK_NEAR(hrEnvDecay, REF_HR_ENV_DECAY, 1e-6);
}

// =====================================================================
//  22-23 highPass / lowPass (contra scipy.signal.lfilter)
// =====================================================================
TEST_CASE(C1_22_N, "highPass", "N", "escalón y seno de 1.2 Hz iguales a scipy (error relativo <= 1e-4)") {
  CHECK(errorMaxFiltro(true, UNO, REF_HP_STEP, 200, CH_IR) <= 1e-4);
  hpZ1[CH_IR] = hpZ2[CH_IR] = 0;
  CHECK(errorMaxFiltro(true, REF_SINE12_IN, REF_HP_SINE12, 300, CH_IR) <= 1e-4);
}
TEST_CASE(C1_22_L, "highPass", "L", "canal RED no altera al IR; estado en cero devuelve b0*x") {
  for (int k = 0; k < 50; k++) highPass(CH_RED, 1000.0f);
  CHECK_EQ(hpZ1[CH_IR], 0.0f);
  CHECK_EQ(hpZ2[CH_IR], 0.0f);
  CHECK_NEAR(highPass(CH_IR, 1.0f), hpB0, 1e-9);
}
TEST_CASE(C1_22_E, "highPass", "E", "entrada 262143 sostenida 2000 muestras: finita, acotada y tiende a 0") {
  float y = 0, ymax = 0;
  for (int k = 0; k < 2000; k++) { y = highPass(CH_IR, 262143.0f); ymax = std::max(ymax, fabsf(y)); CHECK(std::isfinite(y)); if (!std::isfinite(y)) break; }
  CHECK(ymax <= 2.0f * 262143.0f);
  CHECK(fabsf(y) < 1.0f);
}
TEST_CASE(C1_23_N, "lowPass", "N", "escalón, seno de 1.2 Hz y seno de 15 Hz iguales a scipy; 15 Hz atenuado") {
  CHECK(errorMaxFiltro(false, UNO, REF_LP_STEP, 200, CH_IR) <= 1e-4);
  lpZ1[CH_IR] = lpZ2[CH_IR] = 0;
  CHECK(errorMaxFiltro(false, REF_SINE12_IN, REF_LP_SINE12, 300, CH_IR) <= 1e-4);
  lpZ1[CH_IR] = lpZ2[CH_IR] = 0;
  CHECK(errorMaxFiltro(false, REF_SINE15_IN, REF_LP_SINE15, 300, CH_IR) <= 1e-4);
  double amp = 0;
  for (int k = 200; k < 300; k++) amp = std::max(amp, fabs(REF_LP_SINE15[k]));
  CHECK(amp < 0.15);
}
TEST_CASE(C1_23_L, "lowPass", "L", "canal RED no altera al IR; estado en cero devuelve b0*x") {
  for (int k = 0; k < 50; k++) lowPass(CH_RED, 1000.0f);
  CHECK_EQ(lpZ1[CH_IR], 0.0f);
  CHECK_EQ(lpZ2[CH_IR], 0.0f);
  CHECK_NEAR(lowPass(CH_IR, 1.0f), lpB0, 1e-9);
}
TEST_CASE(C1_23_E, "lowPass", "E", "entrada 262143 sostenida: finita y tiende a 262143") {
  float y = 0;
  for (int k = 0; k < 2000; k++) { y = lowPass(CH_IR, 262143.0f); CHECK(std::isfinite(y)); if (!std::isfinite(y)) break; }
  CHECK_NEAR(y, 262143.0, 262143.0 * 1e-4);
}

// =====================================================================
//  24-29 Estadística de intervalos del BPM
// =====================================================================
TEST_CASE(C1_24_N, "hrMeanInterval", "N", "[800,1000,1200] -> 1000 ms") {
  float v[3] = {800, 1000, 1200}; fijarHistorial(v, 3);
  CHECK_NEAR(hrMeanInterval(), 1000.0, 1e-3);
}
TEST_CASE(C1_24_L, "hrMeanInterval", "L", "1 valor; 8 valores (búfer lleno)") {
  float a[1] = {750}; fijarHistorial(a, 1);
  CHECK_NEAR(hrMeanInterval(), 750.0, 1e-3);
  float b[8] = {700, 800, 900, 1000, 1100, 1200, 1300, 1400}; fijarHistorial(b, 8);
  CHECK_NEAR(hrMeanInterval(), 1050.0, 1e-3);
}
TEST_CASE(C1_24_E, "hrMeanInterval", "E", "historial vacío -> 0") {
  CHECK_EQ(hrMeanInterval(), 0.0f);
}
TEST_CASE(C1_25_N, "hrMedianInterval", "N", "[1200,800,1000] desordenado -> 1000; el historial no cambia") {
  float v[3] = {1200, 800, 1000}; fijarHistorial(v, 3);
  CHECK_NEAR(hrMedianInterval(), 1000.0, 1e-3);
  CHECK(hrHistory[0] == 1200 && hrHistory[1] == 800);
}
TEST_CASE(C1_25_L, "hrMedianInterval", "L", "8 valores (par): promedio de los dos centrales = 1050") {
  float b[8] = {1400, 700, 1300, 800, 1200, 900, 1100, 1000}; fijarHistorial(b, 8);
  CHECK_NEAR(hrMedianInterval(), 1050.0, 1e-3);
}
TEST_CASE(C1_25_E, "hrMedianInterval", "E", "historial vacío -> 0") {
  CHECK_EQ(hrMedianInterval(), 0.0f);
}
TEST_CASE(C1_26_N, "hrComputeCv", "N", "[800,1000,1200] -> CV 0.2") {
  float v[3] = {800, 1000, 1200}; fijarHistorial(v, 3);
  CHECK_NEAR(hrComputeCv(), 0.2, 1e-6);
}
TEST_CASE(C1_26_L, "hrComputeCv", "L", "1 valor -> 0") {
  float a[1] = {900}; fijarHistorial(a, 1);
  CHECK_EQ(hrComputeCv(), 0.0f);
}
TEST_CASE(C1_26_E, "hrComputeCv", "E", "historial vacío -> 0") {
  CHECK_EQ(hrComputeCv(), 0.0f);
}
TEST_CASE(C1_27_N, "hrStableBpm", "N", "media 1000 ms -> 60 BPM; 500 ms -> 120 BPM") {
  float a[2] = {1000, 1000}; fijarHistorial(a, 2);
  CHECK_NEAR(hrStableBpm(), 60.0, 1e-3);
  float b[2] = {500, 500}; fijarHistorial(b, 2);
  CHECK_NEAR(hrStableBpm(), 120.0, 1e-3);
}
TEST_CASE(C1_27_E, "hrStableBpm", "E", "historial vacío -> 0") {
  CHECK_EQ(hrStableBpm(), 0.0f);
}
TEST_CASE(C1_28_N, "hrRefractorySamples", "N", "sin historial 30; media 1000 ms 60; media 1500 ms 90 (fs 100)") {
  CHECK_NEAR(hrRefractorySamples(), 30.0, 1e-3);
  float a[1] = {1000}; fijarHistorial(a, 1);
  CHECK_NEAR(hrRefractorySamples(), 60.0, 1e-3);
  float b[1] = {1500}; fijarHistorial(b, 1);
  CHECK_NEAR(hrRefractorySamples(), 90.0, 1e-3);
}
TEST_CASE(C1_28_L, "hrRefractorySamples", "L", "proporcional a fs: 95 -> 28.5; 105 -> 31.5") {
  measuredFs = 95.0f;  CHECK_NEAR(hrRefractorySamples(), 28.5, 1e-3);
  measuredFs = 105.0f; CHECK_NEAR(hrRefractorySamples(), 31.5, 1e-3);
}
TEST_CASE(C1_28_L2, "hrRefractorySamples", "L", "con historial corto (media 400 ms: 60 % = 240 ms < 300 ms) gana el mínimo -> 30") {
  float a[1] = {400}; fijarHistorial(a, 1);
  CHECK_NEAR(hrRefractorySamples(), 30.0, 1e-3);
  float b[1] = {500}; fijarHistorial(b, 1);                // 60 % = 300 ms: igual al mínimo
  CHECK_NEAR(hrRefractorySamples(), 30.0, 1e-3);
}
TEST_CASE(C1_29_N, "hrTimeoutSamples", "N", "sin historial 300; media 1000 ms 300 (gana el mínimo de 3 s); 1500 ms 375") {
  CHECK_NEAR(hrTimeoutSamples(), 300.0, 1e-3);
  float a[1] = {1000}; fijarHistorial(a, 1);
  CHECK_NEAR(hrTimeoutSamples(), 300.0, 1e-3);
  float b[1] = {1500}; fijarHistorial(b, 1);
  CHECK_NEAR(hrTimeoutSamples(), 375.0, 1e-3);
}
TEST_CASE(C1_29_L, "hrTimeoutSamples", "L", "proporcional a fs: 95 -> 285; 105 -> 315") {
  measuredFs = 95.0f;  CHECK_NEAR(hrTimeoutSamples(), 285.0, 1e-3);
  measuredFs = 105.0f; CHECK_NEAR(hrTimeoutSamples(), 315.0, 1e-3);
}

// =====================================================================
//  30 hrValleyBefore
// =====================================================================
static void llenarBufer(uint32_t desde, uint32_t hasta, float valor) {
  for (uint32_t i = desde; i <= hasta; i++) hrPpgBuf[i % HR_VALLEY_WINDOW] = valor;
}
TEST_CASE(C1_30_N, "hrValleyBefore", "N", "búfer 1000-1100 con valle de -5 en 1050 -> -5") {
  llenarBufer(1000, 1100, 10.0f); hrPpgBuf[1050 % HR_VALLEY_WINDOW] = -5.0f;
  hrBufStartIdx = 0; hrValleyStartIdx = 1000;
  CHECK_EQ(hrValleyBefore(1100), -5.0f);
}
TEST_CASE(C1_30_L, "hrValleyBefore", "L", "ventana máxima 159 (incluye pico-159); inicio después del pico; envoltura en 160") {
  // (a) sin historial la ventana es de 159 muestras: pico - 159 = 941 está incluido.
  //     (Un índice anterior no se puede probar: el búfer mide 160 y caería en la
  //     misma casilla que el pico.)
  llenarBufer(900, 1100, 10.0f); hrPpgBuf[941 % HR_VALLEY_WINDOW] = -3.0f;
  hrBufStartIdx = 0; hrValleyStartIdx = 0;
  CHECK_EQ(hrValleyBefore(1100), -3.0f);
  // (b) inicio posterior al pico -> valor del propio pico
  hrValleyStartIdx = 1101; hrPpgBuf[1100 % HR_VALLEY_WINDOW] = 7.0f;
  CHECK_EQ(hrValleyBefore(1100), 7.0f);
  // (c) envoltura del búfer circular (índices 150..170 cruzan 160)
  llenarBufer(150, 170, 4.0f); hrPpgBuf[159 % HR_VALLEY_WINDOW] = -8.0f;
  hrValleyStartIdx = 150;
  CHECK_EQ(hrValleyBefore(170), -8.0f);
}
TEST_CASE(C1_30_E, "hrValleyBefore", "E", "con historial (media 500 ms) la ventana se reduce a 55 muestras") {
  float h[1] = {500}; fijarHistorial(h, 1);                // 1.1 * 500 ms * 100 / 1000 = 55
  llenarBufer(900, 1100, 10.0f); hrPpgBuf[1040 % HR_VALLEY_WINDOW] = -2.0f;
  hrBufStartIdx = 0; hrValleyStartIdx = 0;
  CHECK_EQ(hrValleyBefore(1100), 10.0f);                 // 1100 - 55 = 1045: 1040 excluido
  hrPpgBuf[1050 % HR_VALLEY_WINDOW] = -1.0f;
  CHECK_EQ(hrValleyBefore(1100), -1.0f);                 // 1050 incluido
}
TEST_CASE(C1_30_L2, "hrValleyBefore", "L", "historial largo no amplía la ventana; inicio limitado por hrBufStartIdx; pico < ventana") {
  // (a) media 2000 ms -> 1.1 ciclos = 220 muestras > 159: la ventana sigue en 159
  float h[1] = {2000}; fijarHistorial(h, 1);
  llenarBufer(900, 1100, 10.0f); hrPpgBuf[941 % HR_VALLEY_WINDOW] = -3.0f;
  hrBufStartIdx = 0; hrValleyStartIdx = 0;
  CHECK_EQ(hrValleyBefore(1100), -3.0f);
  // (b) hrValleyStartIdx anterior a hrBufStartIdx: el inicio se ajusta a hrBufStartIdx
  fijarHistorial(h, 0);
  llenarBufer(985, 1100, 10.0f); hrPpgBuf[990 % HR_VALLEY_WINDOW] = -6.0f;
  hrBufStartIdx = 1000; hrValleyStartIdx = 900;
  CHECK_EQ(hrValleyBefore(1100), 10.0f);                 // 990 queda fuera
  hrPpgBuf[1000 % HR_VALLEY_WINDOW] = -4.0f;
  CHECK_EQ(hrValleyBefore(1100), -4.0f);                 // 1000 incluido
  // (c) pico con índice menor que la ventana (arranque): se recorre desde 0
  llenarBufer(0, 50, 3.0f); hrPpgBuf[0] = -9.0f;
  hrBufStartIdx = 0; hrValleyStartIdx = 0;
  CHECK_EQ(hrValleyBefore(50), -9.0f);
}

// =====================================================================
//  31-32 Historial de R
// =====================================================================
TEST_CASE(C1_31_N, "spMedianR", "N", "8 valores de R -> su mediana") {
  float v[8] = {0.40f, 0.42f, 0.38f, 0.45f, 0.41f, 0.43f, 0.39f, 0.44f};
  for (int k = 0; k < 8; k++) spR[k] = v[k];
  spRCount = 8;
  CHECK_NEAR(spMedianR(), (0.41 + 0.42) / 2.0, 1e-6);
}
TEST_CASE(C1_31_E, "spMedianR", "E", "sin valores -> 0") {
  spRCount = 0;
  CHECK_EQ(spMedianR(), 0.0f);
}
TEST_CASE(C1_32_N, "spCvR", "N", "igual a spCvOf sobre el historial") {
  float v[4] = {0.40f, 0.42f, 0.38f, 0.44f};
  for (int k = 0; k < 4; k++) spR[k] = v[k];
  spRCount = 4;
  CHECK_NEAR(spCvR(), spCvOf(v, 4), 1e-9);
}
TEST_CASE(C1_32_E, "spCvR", "E", "1 valor -> 0") {
  spR[0] = 0.4f; spRCount = 1;
  CHECK_EQ(spCvR(), 0.0f);
}

// =====================================================================
//  33-38 Estadísticas de la prueba guiada t
// =====================================================================
TEST_CASE(C1_33_N, "testMeanDc", "N", "suma/cuenta en los bloques 0, 1 y 2") {
  for (int k = 0; k < 3; k++) { tCount[k] = 100; tDcSum[k][CH_IR] = 100.0 * (k + 1) * 1000; }
  for (int k = 0; k < 3; k++) CHECK_NEAR(testMeanDc(k, CH_IR), 1000.0 * (k + 1), 1e-3);
}
TEST_CASE(C1_33_E, "testMeanDc", "E", "cuenta 0 -> 0") {
  tCount[1] = 0; tDcSum[1][CH_IR] = 123;
  CHECK_EQ(testMeanDc(1, CH_IR), 0.0f);
}
TEST_CASE(C1_34_N, "testMeanRaw", "N", "suma/cuenta del valor crudo") {
  tCount[2] = 4; tRawSum[2][CH_RED] = 400000;
  CHECK_NEAR(testMeanRaw(2, CH_RED), 100000.0, 1e-3);
}
TEST_CASE(C1_34_E, "testMeanRaw", "E", "cuenta 0 -> 0") {
  tCount[0] = 0; tRawSum[0][CH_RED] = 55;
  CHECK_EQ(testMeanRaw(0, CH_RED), 0.0f);
}
TEST_CASE(C1_35_N, "testAcRms", "N", "suma de cuadrados 400 en 4 muestras -> 10") {
  tAcCount[1] = 4; tAcSumSq[1][CH_IR] = 400;
  CHECK_NEAR(testAcRms(1, CH_IR), 10.0, 1e-5);
}
TEST_CASE(C1_35_E, "testAcRms", "E", "cuenta 0 -> 0") {
  tAcCount[1] = 0;
  CHECK_EQ(testAcRms(1, CH_IR), 0.0f);
}
static void fijarSubventanas(const float *v, int n) {
  subWindows = n;
  for (int i = 0; i < n; i++) subPP[i][CH_IR] = v[i];
}
TEST_CASE(C1_36_N, "subMeanPP", "N", "[100,120,80] -> 100") {
  float v[3] = {100, 120, 80}; fijarSubventanas(v, 3);
  CHECK_NEAR(subMeanPP(CH_IR), 100.0, 1e-4);
}
TEST_CASE(C1_36_L, "subMeanPP", "L", "12 subventanas (máximo) -> media de las 12") {
  float v[12]; for (int i = 0; i < 12; i++) v[i] = 10.0f * (i + 1);
  fijarSubventanas(v, MAX_SUBWINDOWS);
  CHECK_NEAR(subMeanPP(CH_IR), 65.0, 1e-4);
}
TEST_CASE(C1_36_E, "subMeanPP", "E", "0 subventanas -> 0") {
  subWindows = 0;
  CHECK_EQ(subMeanPP(CH_IR), 0.0f);
}
TEST_CASE(C1_37_N, "subMinPP", "N", "[100,120,80] -> 80") {
  float v[3] = {100, 120, 80}; fijarSubventanas(v, 3);
  CHECK_EQ(subMinPP(CH_IR), 80.0f);
}
TEST_CASE(C1_37_E, "subMinPP", "E", "0 subventanas -> 0") {
  subWindows = 0;
  CHECK_EQ(subMinPP(CH_IR), 0.0f);
}
TEST_CASE(C1_38_N, "subCvPP", "N", "[100,120,80] -> CV 0.2") {
  float v[3] = {100, 120, 80}; fijarSubventanas(v, 3);
  CHECK_NEAR(subCvPP(CH_IR), 0.2, 1e-6);
}
TEST_CASE(C1_38_L, "subCvPP", "L", "1 subventana -> 0") {
  float v[1] = {100}; fijarSubventanas(v, 1);
  CHECK_EQ(subCvPP(CH_IR), 0.0f);
}
TEST_CASE(C1_38_E, "subCvPP", "E", "media <= 0 -> 99") {
  float v[3] = {0, 0, 0}; fijarSubventanas(v, 3);
  CHECK_EQ(subCvPP(CH_IR), 99.0f);
}

// =====================================================================
//  39-40 Fases de las pruebas guiadas
// =====================================================================
TEST_CASE(C1_39_N, "activeTestBlock", "N", "fases de medición -> bloques 0, 1, 2") {
  testPhase = TEST_BASELINE_MEASURE; CHECK_EQ(activeTestBlock(), 0);
  testPhase = TEST_FINGER_MEASURE;   CHECK_EQ(activeTestBlock(), 1);
  testPhase = TEST_REMOVE_MEASURE;   CHECK_EQ(activeTestBlock(), 2);
}
TEST_CASE(C1_39_E, "activeTestBlock", "E", "IDLE y fases de espera -> -1") {
  int fases[4] = {TEST_IDLE, TEST_BASELINE_WAIT, TEST_FINGER_WAIT, TEST_REMOVE_WAIT};
  for (int f : fases) { testPhase = f; CHECK_EQ(activeTestBlock(), -1); }
}
TEST_CASE(C1_40_N, "anyTestRunning", "N", "las 16 combinaciones de las 4 pruebas") {
  for (int m = 0; m < 16; m++) {
    testPhase = (m & 1) ? TEST_FINGER_MEASURE : TEST_IDLE;
    btPhase   = (m & 2) ? BT_STILL : BT_IDLE;
    rtPhase   = (m & 4) ? RT_STILL : RT_IDLE;
    stPhase   = (m & 8) ? ST_STILL : ST_IDLE;
    CHECK_EQ(anyTestRunning(), m != 0);
  }
}

// =====================================================================
//  41 calidadDesdeEstados (tabla de la Etapa 7.3)
// =====================================================================
// Tabla de referencia escrita a partir del plan, NO copiada del firmware
static int calidadEsperada(bool listo, bool dedo, int hr, int ox) {
  if (!listo) return CAL_BUSCANDO;
  if (!dedo) return CAL_SIN_DEDO;
  if (hr == HR_NO_SIGNAL) return CAL_MALA;
  if (hr == HR_STABILIZING) return CAL_BUSCANDO;
  if (ox == OX_VALID) return CAL_BUENA;
  if (ox == OX_STABILIZING) return CAL_BUSCANDO;
  if (ox == OX_LOW_QUALITY || ox == OX_OUT_OF_RANGE) return CAL_REGULAR;
  return CAL_REGULAR;                        // OX_NO_SIGNAL con BPM válido: no especificado en el plan
}
TEST_CASE(C1_41_N, "calidadDesdeEstados", "N", "las 6 filas validadas de la tabla") {
  lastDc[CH_IR] = 100000.0f;
  lastDc[CH_IR] = 1000.0f;  hrState = HR_VALID; oxState = OX_VALID;       CHECK_EQ(calidadDesdeEstados(), CAL_SIN_DEDO);
  lastDc[CH_IR] = 100000.0f;
  hrState = HR_NO_SIGNAL;   oxState = OX_NO_SIGNAL;                       CHECK_EQ(calidadDesdeEstados(), CAL_MALA);
  hrState = HR_STABILIZING; oxState = OX_STABILIZING;                     CHECK_EQ(calidadDesdeEstados(), CAL_BUSCANDO);
  hrState = HR_VALID;       oxState = OX_STABILIZING;                     CHECK_EQ(calidadDesdeEstados(), CAL_BUSCANDO);
  hrState = HR_VALID;       oxState = OX_LOW_QUALITY;                     CHECK_EQ(calidadDesdeEstados(), CAL_REGULAR);
  hrState = HR_VALID;       oxState = OX_OUT_OF_RANGE;                    CHECK_EQ(calidadDesdeEstados(), CAL_REGULAR);
  hrState = HR_VALID;       oxState = OX_VALID;                           CHECK_EQ(calidadDesdeEstados(), CAL_BUENA);
}
TEST_CASE(C1_41_L, "calidadDesdeEstados", "L", "DC IR 50000 = hay dedo; 49999 = SIN DEDO; las 60 combinaciones") {
  hrState = HR_VALID; oxState = OX_VALID;
  lastDc[CH_IR] = 50000.0f; CHECK_EQ(calidadDesdeEstados(), CAL_BUENA);
  lastDc[CH_IR] = 49999.0f; CHECK_EQ(calidadDesdeEstados(), CAL_SIN_DEDO);
  int combinaciones = 0, coinciden = 0;
  for (int listo = 0; listo < 2; listo++)
    for (int dedo = 0; dedo < 2; dedo++)
      for (int hr = 0; hr < 3; hr++)
        for (int ox = 0; ox < 5; ox++) {
          sensorReady = listo; lastDc[CH_IR] = dedo ? 80000.0f : 20000.0f; hrState = hr; oxState = ox;
          combinaciones++;
          if (calidadDesdeEstados() == calidadEsperada(listo, dedo, hr, ox)) coinciden++;
        }
  CHECK_EQ(combinaciones, 60);
  CHECK_EQ(coinciden, 60);
}
TEST_CASE(C1_41_E, "calidadDesdeEstados", "E", "sensor no listo -> BUSCANDO aunque los demás estados digan otra cosa") {
  sensorReady = false; lastDc[CH_IR] = 100000.0f; hrState = HR_VALID; oxState = OX_VALID;
  CHECK_EQ(calidadDesdeEstados(), CAL_BUSCANDO);
}

// =====================================================================
//  42 oxQualityFailReason
// =====================================================================
static void calidadTodoBien() {
  spLastPiIr = 1.0f; spLastAcppRed = 300.0f; spLastCorr = 0.99f; spLastRdiff = 0.02f; spLastDcChange = 0.01f;
}
TEST_CASE(C1_42_N, "oxQualityFailReason", "N", "cada criterio fallando solo da su texto") {
  calidadTodoBien(); spLastPiIr = 0.1f;      CHECK_STR(oxQualityFailReason(), "PI IR bajo");
  calidadTodoBien(); spLastAcppRed = 10.0f;  CHECK_STR(oxQualityFailReason(), "AC RED baja");
  calidadTodoBien(); spLastCorr = 0.5f;      CHECK_STR(oxQualityFailReason(), "correlacion RED-IR baja");
  calidadTodoBien(); spLastRdiff = 0.5f;     CHECK_STR(oxQualityFailReason(), "R_pp y R_rms no concuerdan");
  calidadTodoBien(); spLastDcChange = 0.2f;  CHECK_STR(oxQualityFailReason(), "DC inestable en el latido");
}
TEST_CASE(C1_42_L, "oxQualityFailReason", "L", "umbrales exactos no fallan; un paso fuera sí; prioridad en el orden del código") {
  calidadTodoBien(); spLastPiIr = 0.20f;     CHECK_STR(oxQualityFailReason(), "calidad baja");
  calidadTodoBien(); spLastPiIr = 0.19f;     CHECK_STR(oxQualityFailReason(), "PI IR bajo");
  calidadTodoBien(); spLastAcppRed = 50.0f;  CHECK_STR(oxQualityFailReason(), "calidad baja");
  calidadTodoBien(); spLastAcppRed = 49.0f;  CHECK_STR(oxQualityFailReason(), "AC RED baja");
  calidadTodoBien(); spLastCorr = 0.80f;     CHECK_STR(oxQualityFailReason(), "calidad baja");
  calidadTodoBien(); spLastCorr = 0.79f;     CHECK_STR(oxQualityFailReason(), "correlacion RED-IR baja");
  calidadTodoBien(); spLastRdiff = 0.15f;    CHECK_STR(oxQualityFailReason(), "calidad baja");
  calidadTodoBien(); spLastRdiff = 0.16f;    CHECK_STR(oxQualityFailReason(), "R_pp y R_rms no concuerdan");
  calidadTodoBien(); spLastDcChange = 0.05f; CHECK_STR(oxQualityFailReason(), "calidad baja");
  calidadTodoBien(); spLastDcChange = 0.06f; CHECK_STR(oxQualityFailReason(), "DC inestable en el latido");
  calidadTodoBien(); spLastPiIr = 0.1f; spLastAcppRed = 10.0f;
  CHECK_STR(oxQualityFailReason(), "PI IR bajo");
}
TEST_CASE(C1_42_E, "oxQualityFailReason", "E", "todos los criterios cumplen -> \"calidad baja\" (texto por defecto)") {
  calidadTodoBien();
  CHECK_STR(oxQualityFailReason(), "calidad baja");
}

// =====================================================================
//  43 uiPpgValorAPixel
// =====================================================================
TEST_CASE(C1_43_N, "uiPpgValorAPixel", "N", "valor = centro -> fila 127 (centro de 108-145)") {
  uiPpgCenter = 0.0f; uiPpgSpan = 1000.0f;
  CHECK_EQ(uiPpgValorAPixel(0.0f), 127);
  uiPpgCenter = 200.0f; uiPpgSpan = 40.0f;
  CHECK_EQ(uiPpgValorAPixel(200.0f), 127);
}
TEST_CASE(C1_43_L, "uiPpgValorAPixel", "L", "centro +- rango/2 -> 108 y 145") {
  uiPpgCenter = 0.0f; uiPpgSpan = 1000.0f;
  CHECK_EQ(uiPpgValorAPixel(500.0f), 108);
  CHECK_EQ(uiPpgValorAPixel(-500.0f), 145);
}
TEST_CASE(C1_43_E, "uiPpgValorAPixel", "E", "fuera de escala se recorta a 108 o 145") {
  uiPpgCenter = 0.0f; uiPpgSpan = 1000.0f;
  CHECK_EQ(uiPpgValorAPixel(10000.0f), 108);
  CHECK_EQ(uiPpgValorAPixel(-10000.0f), 145);
}

// =====================================================================
//  44-45 Adaptador de la gráfica
// =====================================================================
TEST_CASE(C1_44_N, "ppgMuestra", "N", "devuelve -spAcIr[idx % 256]") {
  spAcIr[10] = 5.0f;
  CHECK_EQ(ppgMuestra(10), -5.0f);
}
TEST_CASE(C1_44_L, "ppgMuestra", "L", "255; 256 cae en la casilla 0; 2^32-1 cae en la 255") {
  spAcIr[255] = 3.0f; spAcIr[0] = 9.0f;
  CHECK_EQ(ppgMuestra(255), -3.0f);
  CHECK_EQ(ppgMuestra(256), -9.0f);
  CHECK_EQ(ppgMuestra(0xFFFFFFFFu), -3.0f);
}
TEST_CASE(C1_45_N, "ppgMuestraOk", "N", "devuelve spOk[idx % 256]") {
  spOk[20] = true; spOk[21] = false;
  CHECK(ppgMuestraOk(20));
  CHECK(!ppgMuestraOk(21));
}
TEST_CASE(C1_45_L, "ppgMuestraOk", "L", "256 y 2^32-1 respetan la envoltura") {
  spOk[0] = true; spOk[255] = false;
  CHECK(ppgMuestraOk(256));
  CHECK(!ppgMuestraOk(0xFFFFFFFFu));
}

#else  // ======================= VARIANTE SPO2_DECIMALS = 0 =========================

TEST_CASE(C1_13_V0, "formatSpo2Key", "L", "modo entero (SPO2_DECIMALS = 0): 98 -> \" 98\"; -1 -> \" --\"") {
  CHECK_EQ(SPO2_DECIMALS, 0);
  char b[12];
  formatSpo2Key(b, sizeof(b), 98);  CHECK_STR(b, " 98");
  formatSpo2Key(b, sizeof(b), 100); CHECK_STR(b, "100");
  formatSpo2Key(b, sizeof(b), -1);  CHECK_STR(b, " --");
}
TEST_CASE(C1_14_V0, "uiSpo2FieldX", "L", "modo entero: x = 40 y el campo de 3 caracteres termina antes del %") {
  CHECK_EQ(uiSpo2FieldX(), 40);
  CHECK(uiSpo2FieldX() + 3 * 6 * UI_SP_SIZE <= uiPctX());
}
TEST_CASE(C1_15_V0, "uiPctX", "L", "modo entero: el % sigue en x = 98") {
  CHECK_EQ(uiPctX(), 98);
}
#endif

int main() {
#ifdef CAT1_VARIANTE_DEC0
  return mt::runAll("cat1_variante_dec0", REPORT_DIR, antesDeCadaCaso);
#else
  return mt::runAll("cat1", REPORT_DIR, antesDeCadaCaso);
#endif
}
