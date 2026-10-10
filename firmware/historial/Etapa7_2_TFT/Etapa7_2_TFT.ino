/*
 * =====================================================================
 *  ETAPA 7.2 - Interfaz gráfica de la TFT con valores SIMULADOS
 * =====================================================================
 *  Proyecto : Monitor experimental de frecuencia cardiaca y SpO2
 *  Materia  : Sistemas de Información Hospitalaria
 *             Maestría en Ingeniería Clínica
 *  Placa    : NodeMCU ESP-32S (ESP32-WROOM-32)
 *  Pantalla : "1.8 TFT SPI 128*160 V1.1.1" (ST7735S), validada en Etapa 7.1
 *  Librerías: Adafruit ST7735 and ST7789 Library, Adafruit GFX, Adafruit BusIO
 *
 *  PROTOTIPO ACADÉMICO. Los valores mostrados son SIMULADOS y, cuando se
 *  integren los algoritmos, serán ESTIMACIONES EXPERIMENTALES SIN VALIDEZ
 *  CLÍNICA. El decimal de SpO2 NO indica precisión clínica.
 *
 *  Sketch INDEPENDIENTE: NO usa el MAX30102, NO usa WiFi/Bluetooth y NO
 *  modifica ningún algoritmo de las etapas 1-6.2. No hay alarmas.
 *
 *  Conexiones (idénticas a la Etapa 7.1):
 *    TFT VCC -> 3V3   TFT GND -> GND    TFT LED -> 3V3
 *    TFT SCK -> 18    TFT SDA -> 23     TFT A0  -> 16
 *    TFT RESET -> 17  TFT CS  -> 5      (sin MISO, microSD sin usar)
 *
 *  Interfaz pública (preparada para los valores reales de 5 y 6.2):
 *    actualizarBPM(int bpm, bool valido)
 *    actualizarSpO2(float spo2, bool valido)
 *    actualizarCalidad(int calidad)
 *  Cada una redibuja SOLO su campo y SOLO si cambia lo que se ve.
 *  fillScreen() se usa únicamente en uiInit() / uiRedibujarTodo().
 *
 *  Monitor Serie a 115200 baudios. Comandos:
 *    p -> pausar/reanudar simulación   q -> cambiar calidad
 *    x -> alternar valores no válidos  t -> prueba de anchos extremos
 *    m -> alternar pie (simulado/experimental)
 *    f -> redibujado completo          d -> diagnóstico   h -> ayuda
 * =====================================================================
 */

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

// =================================================================
//  CONFIGURACIÓN DE HARDWARE (validada en Etapa 7.1, NO cambiar)
// =================================================================
const int8_t   TFT_SCK         = 18;
const int8_t   TFT_MOSI        = 23;
const int8_t   TFT_MISO_UNUSED = 19;    // reservado por SPI; no se conecta
const int8_t   TFT_DC          = 16;    // A0
const int8_t   TFT_RST         = 17;
const int8_t   TFT_CS          = 5;
const uint32_t TFT_SPI_HZ      = 16000000UL;
const uint32_t SERIAL_BAUD     = 115200;

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// =================================================================
//  OPCIONES DE PRESENTACIÓN
// =================================================================
// 1 = SpO2 con un decimal (98.5 %), 0 = SpO2 entero (98 %).
// Cambiar solo esta constante; las coordenadas se ajustan solas.
const int      SPO2_DECIMALS        = 1;
// Intervalo mínimo entre dos redibujados del MISMO campo. Si llega un
// cambio antes, queda pendiente y uiService() lo dibuja al cumplirse.
const uint32_t UI_MIN_INTERVAL_MS   = 500;

// Códigos de calidad (constantes int: compatibles con el preprocesador)
const int CAL_BUSCANDO = 0;
const int CAL_SIN_DEDO = 1;
const int CAL_MALA     = 2;
const int CAL_REGULAR  = 3;
const int CAL_BUENA    = 4;
const int NUM_CAL      = 5;

// Modos del pie de pantalla
const int MODO_SIMULADO     = 0;
const int MODO_EXPERIMENTAL = 1;

// =================================================================
//  COLORES (RGB565)
// =================================================================
const uint16_t COL_FONDO     = ST77XX_BLACK;
const uint16_t COL_ENCAB     = 0x000F;   // azul marino
const uint16_t COL_TITULO    = ST77XX_WHITE;
const uint16_t COL_ETIQUETA  = 0xC618;   // gris claro
const uint16_t COL_LINEA     = 0x630C;   // gris oscuro
const uint16_t COL_INVALIDO  = 0x8410;   // gris medio
const uint16_t COL_BPM       = ST77XX_GREEN;
const uint16_t COL_SPO2      = ST77XX_CYAN;
const uint16_t COL_PIE       = ST77XX_YELLOW;

// =================================================================
//  DISEÑO: COORDENADAS (rotación 0, 128 x 160)
//  Fuente clásica GFX: 6 x 8 px por carácter x tamaño.
// =================================================================
const int UI_HDR_H      = 14;                    // franja 0-13
const int UI_TITLE_X    = 31,  UI_TITLE_Y   = 3;   // "MONITOR PPG"
const int UI_HRLBL_X    = 7,   UI_HRLBL_Y   = 17;  // "FRECUENCIA CARDIACA"
const int UI_BPM_X      = 16,  UI_BPM_Y     = 28;  // campo 3 car., tamaño 4 (28-59)
const int UI_BPM_SIZE   = 4;
const int UI_BPMU_X     = 55,  UI_BPMU_Y    = 62;  // "BPM"
const int UI_LINE1_Y    = 73;
const int UI_SPLBL_X    = 25,  UI_SPLBL_Y   = 77;  // "SpO2 ESTIMADA"
const int UI_SP_Y       = 88;                      // campo tamaño 3 (88-111)
const int UI_SP_SIZE    = 3;
const int UI_PCT_Y      = 95;                      // "%" tamaño 2, base alineada
const int UI_LINE2_Y    = 115;
const int UI_QLBL_X     = 49,  UI_QLBL_Y    = 119; // "SEÑAL"
const int UI_Q_X        = 16,  UI_Q_Y       = 129; // campo 8 car., tamaño 2 (129-144)
const int UI_Q_SIZE     = 2;
const int UI_Q_CHARS    = 8;
const int UI_LINE3_Y    = 148;
const int UI_PIE_X      = 1,   UI_PIE_Y     = 151; // campo 21 car., tamaño 1
const int UI_PIE_CHARS  = 21;

// =================================================================
//  ESTADO DE LA INTERFAZ
//  Cada campo guarda una "clave" = lo que se VE (no el valor crudo):
//    BPM : valor entero, o -1 si no válido
//    SpO2: décimas (985) o entero (98), o -1 si no válido
//    Cal.: código de calidad      Pie: modo
// =================================================================
const int UI_F_BPM  = 0;
const int UI_F_SPO2 = 1;
const int UI_F_CAL  = 2;
const int UI_F_PIE  = 3;
const int UI_NUM_F  = 4;
const int UI_KEY_NONE = -32768;          // "nunca dibujado"

int      uiDesiredKey[UI_NUM_F];         // lo que se debería ver
int      uiShownKey[UI_NUM_F];           // lo que se ve en pantalla
uint32_t uiLastDrawMs[UI_NUM_F];
uint32_t uiCalls[UI_NUM_F];              // llamadas a actualizar...()
uint32_t uiDraws[UI_NUM_F];              // redibujados reales
uint32_t uiLastUs[UI_NUM_F];
uint32_t uiMaxUs[UI_NUM_F];
uint32_t uiSumUs[UI_NUM_F];
uint32_t uiFillScreenCount = 0;          // solo uiInit / uiRedibujarTodo
uint32_t uiFullRedrawUs    = 0;

// =================================================================
//  ESTADO DE LA SIMULACIÓN
// =================================================================
const uint32_t SIM_STEP_MS       = 1000;    // recalcula cada 1 s
const uint32_t SIM_FIXED_MS      = 10000;   // fase fija inicial: 72 / 98.0 / BUENA
const float    SIM_BPM_CENTER    = 76.5f;   // 68-85 BPM
const float    SIM_BPM_AMPL      = 8.5f;
const float    SIM_BPM_PERIOD_S  = 60.0f;
const float    SIM_SPO2_CENTER   = 98.0f;   // 96-100 %
const float    SIM_SPO2_AMPL     = 2.0f;
const float    SIM_SPO2_PERIOD_S = 90.0f;
const float    SIM_BPM_FIXED     = 72.0f;
const float    SIM_SPO2_FIXED    = 98.0f;
const uint32_t STATUS_PERIOD_MS  = 5000;

bool     simPaused      = false;
bool     simInvalid     = false;
int      simCalidad     = CAL_BUENA;
uint32_t simElapsedMs   = 0;               // tiempo simulado (se congela en pausa)
uint32_t simLastMs      = 0;
uint32_t simLastStepMs  = 0;
float    simBpm         = SIM_BPM_FIXED;
float    simSpo2        = SIM_SPO2_FIXED;
int      uiModo         = MODO_SIMULADO;
uint32_t lastStatusMs   = 0;

// Prueba de anchos extremos ('t')
const uint32_t EDGE_STEP_MS = 2500;
const int      EDGE_STEPS   = 5;
const int      EDGE_BPM[EDGE_STEPS]   = {40, 100, 180, 99, 0};
const float    EDGE_SPO2[EDGE_STEPS]  = {70.0f, 100.0f, 85.3f, 99.9f, 0.0f};
const bool     EDGE_VALID[EDGE_STEPS] = {true, true, true, true, false};
const int      EDGE_CAL[EDGE_STEPS]   = {CAL_MALA, CAL_REGULAR, CAL_BUENA, CAL_BUSCANDO, CAL_SIN_DEDO};
bool     edgeActive  = false;
int      edgeIndex   = 0;
uint32_t edgeStepMs  = 0;
bool     edgeWasPaused = false;

// =================================================================
//  UTILIDADES DE TEXTO
// =================================================================
const char *calidadTexto(int calidad) {
  switch (calidad) {
    case CAL_BUENA:    return "BUENA";
    case CAL_REGULAR:  return "REGULAR";
    case CAL_MALA:     return "MALA";
    case CAL_SIN_DEDO: return "SIN DEDO";
    default:           return "BUSCANDO";
  }
}

uint16_t calidadColor(int calidad) {
  switch (calidad) {
    case CAL_BUENA:   return ST77XX_GREEN;
    case CAL_REGULAR: return ST77XX_YELLOW;
    case CAL_MALA:    return ST77XX_ORANGE;
    default:          return COL_ETIQUETA;   // SIN DEDO / BUSCANDO
  }
}

const char *modoTexto(int modo) {
  return (modo == MODO_EXPERIMENTAL) ? "ESTIM. EXPERIMENTAL" : "DATOS SIMULADOS";
}

const char *campoNombre(int f) {
  switch (f) {
    case UI_F_BPM:  return "BPM    ";
    case UI_F_SPO2: return "SpO2   ";
    case UI_F_CAL:  return "Calidad";
    default:        return "Pie    ";
  }
}

// Centra 'texto' en un campo de 'ancho' caracteres rellenando con
// espacios (los espacios, con fondo opaco, borran restos anteriores).
void uiTextoCentrado(char *out, int ancho, const char *texto) {
  int n = (int)strlen(texto);
  if (n > ancho) n = ancho;
  int izq = (ancho - n) / 2;
  int i = 0;
  for (; i < izq; i++) out[i] = ' ';
  for (int k = 0; k < n; k++) out[i++] = texto[k];
  for (; i < ancho; i++) out[i] = ' ';
  out[ancho] = '\0';
}

// Texto visible del campo SpO2 a partir de su clave (ancho fijo).
// Se usa tanto para dibujar como para el reporte serie.
void formatSpo2Key(char *buf, int bufLen, int key) {
  if (SPO2_DECIMALS > 0) {
    if (key < 0) snprintf(buf, bufLen, " --.-");
    else         snprintf(buf, bufLen, "%3d.%d", key / 10, key % 10);
  } else {
    if (key < 0) snprintf(buf, bufLen, " --");
    else         snprintf(buf, bufLen, "%3d", key);
  }
}

// Coordenadas del campo SpO2 según SPO2_DECIMALS
int uiSpo2FieldX()  { return (SPO2_DECIMALS > 0) ? 4 : 22; }   // " 98.5" (5 car.) o " 98" (3 car.)
int uiPctX()        { return (SPO2_DECIMALS > 0) ? 98 : 80; }

// =================================================================
//  CAPA DE DIBUJO (único lugar que escribe en la TFT)
// =================================================================
void uiDibujarEstatica() {
  tft.fillRect(0, 0, 128, UI_HDR_H, COL_ENCAB);
  tft.setTextSize(1);
  tft.setTextColor(COL_TITULO, COL_ENCAB);
  tft.setCursor(UI_TITLE_X, UI_TITLE_Y);
  tft.print("MONITOR PPG");

  tft.setTextColor(COL_ETIQUETA, COL_FONDO);
  tft.setCursor(UI_HRLBL_X, UI_HRLBL_Y);
  tft.print("FRECUENCIA CARDIACA");
  tft.setCursor(UI_BPMU_X, UI_BPMU_Y);
  tft.print("BPM");
  tft.drawFastHLine(8, UI_LINE1_Y, 112, COL_LINEA);

  tft.setCursor(UI_SPLBL_X, UI_SPLBL_Y);
  tft.print("SpO2 ESTIMADA");
  tft.drawFastHLine(8, UI_LINE2_Y, 112, COL_LINEA);

  tft.setCursor(UI_QLBL_X, UI_QLBL_Y);
  tft.print("SE");
  tft.write((uint8_t)0xA5);           // 'Ñ' en la tabla CP437 de la fuente GFX
  tft.print("AL");
  tft.drawFastHLine(0, UI_LINE3_Y, 128, COL_LINEA);
}

void uiDibujarCampoBPM(int key) {
  char buf[12];
  uint16_t color = COL_BPM;
  if (key < 0) { strcpy(buf, " --"); color = COL_INVALIDO; }
  else         { snprintf(buf, sizeof(buf), "%3d", key); }
  tft.setTextSize(UI_BPM_SIZE);
  tft.setTextColor(color, COL_FONDO);     // texto OPACO: sin borrar antes
  tft.setCursor(UI_BPM_X, UI_BPM_Y);
  tft.print(buf);
}

void uiDibujarCampoSpO2(int key) {
  char buf[12];
  formatSpo2Key(buf, sizeof(buf), key);
  uint16_t color = (key < 0) ? COL_INVALIDO : COL_SPO2;
  tft.setTextSize(UI_SP_SIZE);
  tft.setTextColor(color, COL_FONDO);
  tft.setCursor(uiSpo2FieldX(), UI_SP_Y);
  tft.print(buf);
  // El "%" acompaña al color del valor (gris si no es válido)
  tft.setTextSize(2);
  tft.setTextColor(color, COL_FONDO);
  tft.setCursor(uiPctX(), UI_PCT_Y);
  tft.print("%");
}

void uiDibujarCampoCalidad(int key) {
  char buf[UI_Q_CHARS + 1];
  uiTextoCentrado(buf, UI_Q_CHARS, calidadTexto(key));
  tft.setTextSize(UI_Q_SIZE);
  tft.setTextColor(calidadColor(key), COL_FONDO);
  tft.setCursor(UI_Q_X, UI_Q_Y);
  tft.print(buf);
}

void uiDibujarPie(int key) {
  char buf[UI_PIE_CHARS + 1];
  uiTextoCentrado(buf, UI_PIE_CHARS, modoTexto(key));
  tft.setTextSize(1);
  tft.setTextColor(COL_PIE, COL_FONDO);
  tft.setCursor(UI_PIE_X, UI_PIE_Y);
  tft.print(buf);
}

// Dibuja un campo, mide su tiempo y registra lo que quedó visible
void uiDibujarCampo(int f, uint32_t now) {
  int key = uiDesiredKey[f];
  uint32_t t0 = micros();
  switch (f) {
    case UI_F_BPM:  uiDibujarCampoBPM(key);     break;
    case UI_F_SPO2: uiDibujarCampoSpO2(key);    break;
    case UI_F_CAL:  uiDibujarCampoCalidad(key); break;
    default:        uiDibujarPie(key);          break;
  }
  uint32_t dt = micros() - t0;
  uiShownKey[f]   = key;
  uiLastDrawMs[f] = now;
  uiDraws[f]++;
  uiLastUs[f] = dt;
  uiSumUs[f] += dt;
  if (dt > uiMaxUs[f]) uiMaxUs[f] = dt;
}

// Registra el valor deseado; dibuja solo si lo visible cambia.
void uiSolicitar(int f, int key) {
  uint32_t now = millis();
  uiCalls[f]++;
  uiDesiredKey[f] = key;
  if (key == uiShownKey[f]) return;                     // nada visible cambió
  if (now - uiLastDrawMs[f] >= UI_MIN_INTERVAL_MS) {
    uiDibujarCampo(f, now);                             // solo ESTE campo
  }
  // si no, queda pendiente (deseado != visible) y lo dibuja uiService()
}

// =================================================================
//  INTERFAZ PÚBLICA
// =================================================================
void uiRedibujarTodo() {
  uint32_t now = millis();
  uint32_t t0 = micros();
  tft.fillScreen(COL_FONDO);              // redibujado completo DELIBERADO
  uiFillScreenCount++;
  uiDibujarEstatica();
  for (int f = 0; f < UI_NUM_F; f++) uiDibujarCampo(f, now);
  uiFullRedrawUs = micros() - t0;
}

void uiInit() {
  SPI.begin(TFT_SCK, TFT_MISO_UNUSED, TFT_MOSI, -1);   // VSPI, pines explícitos
  tft.initR(INITR_BLACKTAB);
  tft.setSPISpeed(TFT_SPI_HZ);
  tft.setRotation(0);
  tft.setTextWrap(false);
  tft.cp437(true);                                    // tabla CP437 correcta (Ñ = 0xA5)
  for (int f = 0; f < UI_NUM_F; f++) {
    uiShownKey[f] = UI_KEY_NONE;
    uiLastDrawMs[f] = 0;
    uiCalls[f] = uiDraws[f] = uiLastUs[f] = uiMaxUs[f] = uiSumUs[f] = 0;
  }
  uiDesiredKey[UI_F_BPM]  = -1;                       // hasta recibir datos: "--"
  uiDesiredKey[UI_F_SPO2] = -1;
  uiDesiredKey[UI_F_CAL]  = CAL_BUSCANDO;
  uiDesiredKey[UI_F_PIE]  = uiModo;
  uiRedibujarTodo();
}

// Dibuja los cambios pendientes cuando se cumple el intervalo mínimo.
// Llamar en cada loop(); si no hay pendientes no toca la TFT.
void uiService(uint32_t now) {
  for (int f = 0; f < UI_NUM_F; f++) {
    if (uiDesiredKey[f] != uiShownKey[f] && now - uiLastDrawMs[f] >= UI_MIN_INTERVAL_MS) {
      uiDibujarCampo(f, now);
    }
  }
}

void actualizarBPM(int bpm, bool valido) {
  int key = -1;
  if (valido && bpm >= 0) key = (bpm > 999) ? 999 : bpm;
  uiSolicitar(UI_F_BPM, key);
}

void actualizarSpO2(float spo2, bool valido) {
  int key = -1;
  if (valido && spo2 >= 0.0f) {
    if (spo2 > 100.0f) spo2 = 100.0f;
    key = (SPO2_DECIMALS > 0) ? (int)lroundf(spo2 * 10.0f) : (int)lroundf(spo2);
  }
  uiSolicitar(UI_F_SPO2, key);
}

void actualizarCalidad(int calidad) {
  if (calidad < 0 || calidad >= NUM_CAL) calidad = CAL_BUSCANDO;
  uiSolicitar(UI_F_CAL, calidad);
}

void uiSetModo(int modo) {
  uiModo = (modo == MODO_EXPERIMENTAL) ? MODO_EXPERIMENTAL : MODO_SIMULADO;
  uiSolicitar(UI_F_PIE, uiModo);
}

// =================================================================
//  SIMULADOR (se reemplazará por los algoritmos de las etapas 5 y 6.2)
// =================================================================
void simCompute() {
  if (simElapsedMs < SIM_FIXED_MS) {
    simBpm  = SIM_BPM_FIXED;
    simSpo2 = SIM_SPO2_FIXED;
    return;
  }
  float t = (simElapsedMs - SIM_FIXED_MS) / 1000.0f;
  // Fase inicial elegida para que BPM arranque en 72 (continuidad)
  const float phase = asinf((SIM_BPM_FIXED - SIM_BPM_CENTER) / SIM_BPM_AMPL);
  simBpm  = SIM_BPM_CENTER  + SIM_BPM_AMPL  * sinf(2.0f * PI * t / SIM_BPM_PERIOD_S + phase);
  simSpo2 = SIM_SPO2_CENTER + SIM_SPO2_AMPL * sinf(2.0f * PI * t / SIM_SPO2_PERIOD_S);
}

void simSend() {
  actualizarBPM((int)(simBpm + 0.5f), !simInvalid);
  actualizarSpO2(simSpo2, !simInvalid);
  actualizarCalidad(simCalidad);
}

void simInit(uint32_t now) {
  simElapsedMs  = 0;
  simLastMs     = now;
  simLastStepMs = now;
  simCompute();
  simSend();
}

void simUpdate(uint32_t now) {
  uint32_t dt = now - simLastMs;
  simLastMs = now;
  if (simPaused || edgeActive) return;          // tiempo simulado congelado
  simElapsedMs += dt;
  if (now - simLastStepMs < SIM_STEP_MS) return;
  simLastStepMs = now;
  simCompute();
  simSend();
}

// =================================================================
//  PRUEBA DE ANCHOS EXTREMOS ('t')
// =================================================================
void edgeApply(int i) {
  actualizarBPM(EDGE_BPM[i], EDGE_VALID[i]);
  actualizarSpO2(EDGE_SPO2[i], EDGE_VALID[i]);
  actualizarCalidad(EDGE_CAL[i]);
  if (EDGE_VALID[i]) {
    Serial.printf("   Paso %d/%d: BPM %d | SpO2 %.1f %% | %s\n", i + 1, EDGE_STEPS,
                  EDGE_BPM[i], EDGE_SPO2[i], calidadTexto(EDGE_CAL[i]));
  } else {
    Serial.printf("   Paso %d/%d: valores NO validos (--) | %s\n", i + 1, EDGE_STEPS,
                  calidadTexto(EDGE_CAL[i]));
  }
}

void startEdgeTest(uint32_t now) {
  edgeActive    = true;
  edgeIndex     = 0;
  edgeStepMs    = now;
  edgeWasPaused = simPaused;
  Serial.println("\n[T] PRUEBA DE ANCHOS EXTREMOS (2.5 s por paso; simulacion detenida)");
  Serial.println("   OBSERVE: ningun digito o letra anterior queda 'pegado' al cambiar de ancho");
  Serial.println("   (100 -> 99, 100.0 -> 99.9, REGULAR -> BUENA, etc.).");
  Serial.println("   OBSERVE: solo cambian los tres campos; titulos y lineas NO parpadean.");
  edgeApply(edgeIndex);
}

void updateEdgeTest(uint32_t now) {
  if (!edgeActive || now - edgeStepMs < EDGE_STEP_MS) return;
  edgeStepMs = now;
  edgeIndex++;
  if (edgeIndex < EDGE_STEPS) { edgeApply(edgeIndex); return; }
  edgeActive = false;
  simPaused  = edgeWasPaused;
  Serial.println("   Fin de la prueba; la simulacion continua.\n");
  simSend();                                     // restaura los valores simulados
}

// =================================================================
//  MONITOR SERIE
// =================================================================
void printHelp() {
  Serial.println("\nComandos:");
  Serial.println("  p -> pausar / reanudar la simulacion");
  Serial.println("  q -> cambiar calidad (BUENA > REGULAR > MALA > SIN DEDO > BUSCANDO)");
  Serial.println("  x -> alternar valores NO validos (muestra --)");
  Serial.println("  t -> prueba de anchos extremos (40/100/180 BPM, 70.0/100.0 %)");
  Serial.println("  m -> alternar pie: DATOS SIMULADOS / ESTIM. EXPERIMENTAL");
  Serial.println("  f -> redibujado completo deliberado (uiRedibujarTodo)");
  Serial.println("  d -> diagnostico de tiempos      h -> ayuda\n");
}

void printDiagnostics() {
  Serial.println("\n================ DIAGNOSTICO ================");
  Serial.printf (" TFT: INITR_BLACKTAB, SPI %lu MHz, 128 x 160, rotacion 0\n",
                 (unsigned long)(TFT_SPI_HZ / 1000000UL));
  Serial.printf (" SpO2 con %d decimal(es) | intervalo minimo por campo %lu ms\n",
                 SPO2_DECIMALS, (unsigned long)UI_MIN_INTERVAL_MS);
  Serial.printf (" fillScreen usados: %lu (solo uiInit / uiRedibujarTodo)\n",
                 (unsigned long)uiFillScreenCount);
  Serial.printf (" Ultimo redibujado completo: %.1f ms\n", uiFullRedrawUs / 1000.0f);
  Serial.println(" Campo    llamadas  redibujados  ultimo(us)  max(us)  prom(us)");
  uint32_t worst = 0;
  for (int f = 0; f < UI_NUM_F; f++) {
    uint32_t avg = uiDraws[f] ? uiSumUs[f] / uiDraws[f] : 0;
    Serial.printf (" %s  %8lu  %11lu  %10lu  %7lu  %8lu\n", campoNombre(f),
                   (unsigned long)uiCalls[f], (unsigned long)uiDraws[f], (unsigned long)uiLastUs[f],
                   (unsigned long)uiMaxUs[f], (unsigned long)avg);
    if (f != UI_F_PIE) worst += uiMaxUs[f];
  }
  Serial.printf (" Peor caso BPM+SpO2+Calidad: %.2f ms (margen FIFO MAX30102: 320 ms)\n", worst / 1000.0f);
  Serial.printf (" Simulacion: %s | valores %s | t simulado %lu s\n",
                 simPaused ? "PAUSADA" : "activa", simInvalid ? "NO validos" : "validos",
                 (unsigned long)(simElapsedMs / 1000UL));
  Serial.printf (" Heap libre: %lu bytes\n", (unsigned long)ESP.getFreeHeap());
  Serial.println("=============================================\n");
}

void printStatus(uint32_t now) {
  if (now - lastStatusMs < STATUS_PERIOD_MS) return;
  lastStatusMs = now;
  char sp[12];
  formatSpo2Key(sp, sizeof(sp), uiShownKey[UI_F_SPO2]);
  char bpm[12];
  if (uiShownKey[UI_F_BPM] < 0) snprintf(bpm, sizeof(bpm), " --");
  else                          snprintf(bpm, sizeof(bpm), "%3d", uiShownKey[UI_F_BPM]);
  Serial.printf("[t=%lus] simulado: %.1f BPM, %.2f %% | pantalla: %s BPM, %s %%, %s | redibujados B/S/C: %lu/%lu/%lu\n",
                (unsigned long)(now / 1000UL), simBpm, simSpo2, bpm, sp,
                calidadTexto(uiShownKey[UI_F_CAL]),
                (unsigned long)uiDraws[UI_F_BPM], (unsigned long)uiDraws[UI_F_SPO2],
                (unsigned long)uiDraws[UI_F_CAL]);
}

void handleSerialCommands() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r' || c == '\n' || c == ' ') continue;
    uint32_t now = millis();
    if (edgeActive && c != 'd' && c != 'h') {
      Serial.println(">> Prueba de anchos en curso; espere a que termine.");
      continue;
    }
    if (c == 'p' || c == 'P') {
      simPaused = !simPaused;
      Serial.printf(">> Simulacion %s\n", simPaused ? "PAUSADA (la pantalla no debe cambiar)" : "REANUDADA");
    } else if (c == 'q' || c == 'Q') {
      const int orden[NUM_CAL] = {CAL_BUENA, CAL_REGULAR, CAL_MALA, CAL_SIN_DEDO, CAL_BUSCANDO};
      int pos = 0;
      for (int i = 0; i < NUM_CAL; i++) if (orden[i] == simCalidad) pos = i;
      simCalidad = orden[(pos + 1) % NUM_CAL];
      actualizarCalidad(simCalidad);
      Serial.printf(">> Calidad: %s (solo debe cambiar la palabra inferior)\n", calidadTexto(simCalidad));
    } else if (c == 'x' || c == 'X') {
      simInvalid = !simInvalid;
      simSend();
      Serial.printf(">> Valores %s\n", simInvalid ? "NO validos: BPM y SpO2 deben mostrar -- en gris"
                                                  : "validos: vuelven los numeros en color");
    } else if (c == 't' || c == 'T') {
      startEdgeTest(now);
    } else if (c == 'm' || c == 'M') {
      uiSetModo(uiModo == MODO_SIMULADO ? MODO_EXPERIMENTAL : MODO_SIMULADO);
      Serial.printf(">> Pie: %s\n", modoTexto(uiModo));
    } else if (c == 'f' || c == 'F') {
      uiRedibujarTodo();
      Serial.printf(">> Redibujado completo deliberado: %.1f ms (fillScreen #%lu)\n",
                    uiFullRedrawUs / 1000.0f, (unsigned long)uiFillScreenCount);
    } else if (c == 'd' || c == 'D') {
      printDiagnostics();
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
  Serial.begin(SERIAL_BAUD);
  delay(1000);                                  // solo en setup
  Serial.println();
  Serial.println("================================================");
  Serial.println(" ETAPA 7.2 - Interfaz grafica TFT (valores SIMULADOS)");
  Serial.println(" Prototipo academico. Sin MAX30102, sin WiFi/BT, sin alarmas.");
  Serial.println(" Los valores NO son mediciones; en la integracion seran");
  Serial.println(" ESTIMACIONES EXPERIMENTALES sin validez clinica.");
  Serial.println("================================================");

  uiInit();
  Serial.printf(" Pantalla inicializada; redibujado completo: %.1f ms\n", uiFullRedrawUs / 1000.0f);
  Serial.println(" OBSERVE: al inicio BPM y SpO2 muestran -- y calidad BUSCANDO;");
  Serial.println(" luego 72 BPM, 98.0 % y BUENA fijos 10 s; despues varian lentamente");
  Serial.println(" (68-85 BPM, 96-100 %). Solo cambian los numeros: titulos y lineas");
  Serial.println(" nunca parpadean.");
  printHelp();

  // El primer valor simulado queda pendiente ~0.5 s (intervalo mínimo),
  // por eso al arrancar se alcanzan a ver los "--".
  simInit(millis());
}

void loop() {
  uint32_t now = millis();
  handleSerialCommands();
  updateEdgeTest(now);
  simUpdate(now);
  uiService(now);
  printStatus(now);
}
