/*
 * =====================================================================
 *  ETAPA 7.1 - Verificación de la pantalla TFT ST7735 1.8" (128x160)
 * =====================================================================
 *  Proyecto : Monitor experimental de frecuencia cardiaca y SpO2
 *  Materia  : Sistemas de Información Hospitalaria
 *             Maestría en Ingeniería Clínica
 *  Placa    : NodeMCU ESP-32S (ESP32-WROOM-32)
 *  Pantalla : "1.8 TFT SPI 128*160 V1.1.1" (controlador ST7735S)
 *  Entorno  : Arduino IDE, Arduino-ESP32 3.3.x (ESP-IDF 5.5)
 *  Librerías: Adafruit ST7735 and ST7789 Library (v1.11.0 revisada)
 *             Adafruit GFX Library (+ Adafruit BusIO)
 *
 *  PROTOTIPO ACADÉMICO Y EXPERIMENTAL. Sketch INDEPENDIENTE:
 *  NO usa el MAX30102 y NO modifica las etapas 1-6.2.
 *
 *  Conexiones (MAX30102 DESCONECTADO, microSD sin usar):
 *    TFT VCC   -> 3V3          TFT SCK   -> GPIO 18 (reloj SPI, VSPI)
 *    TFT GND   -> GND          TFT SDA   -> GPIO 23 (MOSI SPI, NO es I2C)
 *    TFT LED   -> 3V3          TFT A0    -> GPIO 16 (DC: dato/comando)
 *                              TFT RESET -> GPIO 17
 *                              TFT CS    -> GPIO 5
 *    El módulo NO tiene MISO: el ESP32 solo puede ESCRIBIR en la pantalla.
 *    Por eso la verificación combina patrones VISUALES y tiempos medidos.
 *
 *  Patrones (cada uno indica por el Monitor Serie QUÉ OBSERVAR):
 *    1 Colores sólidos + tiempo de fillScreen
 *    2 Marco de calibración (resolución y desplazamiento)
 *    3 Barras de color R G B C M Y + degradado (orden de color)
 *    4 Texto en tamaños 1, 2 y 3
 *    5 Figuras geométricas (proporción y esquinas)
 *    6 Rotaciones 0-3
 *    7 Animación (estabilidad corta; 'e' = prueba larga de 5 min)
 *    8 Velocidad de dibujo parcial (presupuesto para la integración)
 *
 *  Monitor Serie a 115200 baudios. Comandos:
 *    n -> siguiente patrón        1..8 -> ir a un patrón
 *    a -> avance automático (activar/desactivar)
 *    v -> alternar variante BLACKTAB / GREENTAB (reinicializa)
 *    i -> invertir colores        r -> rotar (0-3)
 *    f -> cambiar velocidad SPI   s -> barrido de velocidades SPI
 *    e -> estabilidad 5 min       d -> diagnóstico   h -> ayuda
 * =====================================================================
 */

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <esp_system.h>

// =================================================================
//  CONFIGURACIÓN
// =================================================================
// 0 = SPI por hardware (normal). 1 = SPI por software (respaldo de
// diagnóstico, mucho más lento): si la pantalla funciona con 1 pero no
// con 0, el problema es de configuración SPI y no de cableado.
#define USE_SOFTWARE_SPI 0

const int8_t   TFT_SCK          = 18;
const int8_t   TFT_MOSI         = 23;
const int8_t   TFT_MISO_UNUSED  = 19;   // reservado por SPI; no se conecta
const int8_t   TFT_DC           = 16;   // A0
const int8_t   TFT_RST          = 17;
const int8_t   TFT_CS           = 5;

const uint8_t  VARIANT_DEFAULT  = INITR_BLACKTAB;
const int      EXPECTED_WIDTH   = 128;  // en rotación 0
const int      EXPECTED_HEIGHT  = 160;
const uint32_t SERIAL_BAUD      = 115200;

// Velocidades SPI de prueba (MHz). El ESP32 obtiene el reloj de 80 MHz
// con divisores enteros: la frecuencia real puede ser menor a la pedida.
const int      NUM_SPEEDS       = 5;
const uint32_t SPI_SPEEDS_MHZ[NUM_SPEEDS] = {8, 16, 20, 27, 40};
const int      SPEED_DEFAULT_INDEX = 1;  // 16 MHz: conservadora

const int      NUM_PATTERNS          = 8;
const uint32_t AUTO_PATTERN_MS       = 10000;   // avance automático
const uint32_t COLOR_STEP_MS         = 1500;    // patrón 1
const uint32_t ROTATION_STEP_MS      = 3000;    // patrón 6
const uint32_t SWEEP_STEP_MS         = 3000;    // barrido SPI
const uint32_t STABILITY_LONG_MS     = 300000;  // 5 min ('e')
const uint32_t FIFO_BUDGET_MS        = 320;     // margen del FIFO del MAX30102 (Etapa 3)

// =================================================================
//  OBJETO DE LA PANTALLA
// =================================================================
#if USE_SOFTWARE_SPI
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCK, TFT_RST);
#else
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
#endif

// =================================================================
//  ESTADO
// =================================================================
uint8_t  currentVariant   = VARIANT_DEFAULT;
int      currentRotation  = 0;
int      speedIndex       = SPEED_DEFAULT_INDEX;
bool     inverted         = false;
bool     autoAdvance      = false;
int      currentPattern   = 1;
uint32_t patternStartMs   = 0;
int      patternStep      = 0;          // sub-paso de patrones con varias fases
uint32_t stepStartMs      = 0;
uint32_t initTimeUs       = 0;
uint32_t lastFillUs       = 0;

// Animación / estabilidad
bool     stabilityLong    = false;
uint32_t stabilityStartMs = 0;
uint32_t stabilityFrames  = 0;
uint32_t lastFrameMs      = 0;
uint32_t lastBeatMs       = 0;
uint32_t framesAtLastBeat = 0;
int      boxX = 10, boxY = 40, boxDx = 2, boxDy = 3;
int      barX = 0;

// Barrido de velocidad SPI
bool     sweepActive      = false;
int      sweepIndex       = 0;
uint32_t sweepStepMs      = 0;
uint32_t sweepFillUs[NUM_SPEEDS];

// =================================================================
//  UTILIDADES
// =================================================================
const char *variantName(uint8_t v) {
  if (v == INITR_BLACKTAB) return "INITR_BLACKTAB";
  if (v == INITR_GREENTAB) return "INITR_GREENTAB";
  return "desconocida";
}

uint32_t requestedSpiHz() {
  return SPI_SPEEDS_MHZ[speedIndex] * 1000000UL;
}

// Frecuencia real aproximada: 80 MHz / divisor entero (sin superar la pedida)
float actualSpiMHz(uint32_t requestedMHz) {
  uint32_t div = (80 + requestedMHz - 1) / requestedMHz;
  if (div < 2) div = 2;
  return 80.0f / div;
}

// Tiempo teórico de llenar la pantalla: 128 x 160 px x 16 bits / f_SPI
float theoreticalFillMs(float spiMHz) {
  return (float)EXPECTED_WIDTH * EXPECTED_HEIGHT * 16.0f / (spiMHz * 1000.0f);
}

uint32_t timedFill(uint16_t color) {
  uint32_t t0 = micros();
  tft.fillScreen(color);
  lastFillUs = micros() - t0;
  return lastFillUs;
}

const char *resetReasonText(int r) {   // recibe esp_reset_reason() convertido a int
  switch ((esp_reset_reason_t)r) {
    case ESP_RST_POWERON:  return "encendido";
    case ESP_RST_EXT:      return "reset externo";
    case ESP_RST_SW:       return "reinicio por software";
    case ESP_RST_PANIC:    return "error del programa (panic)";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:      return "watchdog";
    case ESP_RST_BROWNOUT: return "CAIDA DE VOLTAJE (brownout)";
    default:               return "otro";
  }
}

void printObserve(const char *text) {
  Serial.print("   OBSERVE: ");
  Serial.println(text);
}

void printFillReport(const char *colorName) {
  float mhz = actualSpiMHz(SPI_SPEEDS_MHZ[speedIndex]);
  float theo = theoreticalFillMs(mhz);
  float meas = lastFillUs / 1000.0f;
  float ratio = meas / theo;
#if USE_SOFTWARE_SPI
  const char *verdict = "  [SPI por software: no comparable]";
#else
  const char *verdict = (ratio >= 0.7f && ratio <= 1.3f) ? "  OK" : "  FUERA DE +-30 %";
#endif
  Serial.printf("   fillScreen %-8s: %6.1f ms (teorico %.1f ms a %.2f MHz, relacion %.2f)%s\n",
                colorName, meas, theo, mhz, ratio, verdict);
}

// =================================================================
//  INICIALIZACIÓN DE LA PANTALLA
// =================================================================
void tftInit(uint8_t variant) {
  currentVariant = variant;
#if !USE_SOFTWARE_SPI
  // Pines del SPI por hardware explícitos (VSPI). Si el bus ya estaba
  // iniciado, SPI.begin() no hace nada.
  SPI.begin(TFT_SCK, TFT_MISO_UNUSED, TFT_MOSI, -1);
#endif
  uint32_t t0 = micros();
  tft.initR(variant);                 // la librería envía la secuencia a 32 MHz
  initTimeUs = micros() - t0;
#if !USE_SOFTWARE_SPI
  tft.setSPISpeed(requestedSpiHz());  // todo el dibujo a la velocidad conservadora
#endif
  tft.setRotation(currentRotation);
  tft.invertDisplay(inverted);
  tft.setTextWrap(false);

  int w = tft.width();
  int h = tft.height();
  bool sizeOk = (currentRotation % 2 == 0) ? (w == EXPECTED_WIDTH && h == EXPECTED_HEIGHT)
                                           : (w == EXPECTED_HEIGHT && h == EXPECTED_WIDTH);
  Serial.println("\n------------------------------------------------");
  Serial.printf (" Inicializacion: %s en %.1f ms\n", variantName(variant), initTimeUs / 1000.0f);
#if USE_SOFTWARE_SPI
  Serial.println(" SPI           : POR SOFTWARE (respaldo de diagnostico)");
#else
  Serial.printf (" SPI           : hardware VSPI, solicitada %lu MHz, real ~%.2f MHz\n",
                 (unsigned long)SPI_SPEEDS_MHZ[speedIndex], actualSpiMHz(SPI_SPEEDS_MHZ[speedIndex]));
#endif
  Serial.printf (" Rotacion %d    : %d x %d px (esperado %s) -> %s\n", currentRotation, w, h,
                 (currentRotation % 2 == 0) ? "128 x 160" : "160 x 128", sizeOk ? "OK" : "DIFERENTE");
  Serial.printf (" Colores invertidos: %s\n", inverted ? "SI" : "NO");
  Serial.println("------------------------------------------------");
}

// =================================================================
//  PATRONES DE PRUEBA
// =================================================================
void drawTitle(const char *title) {
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(2, 2);
  tft.print(title);
}

// ---- Patrón 1: colores sólidos (cambia cada 1.5 s) ----
void pattern1Step(int step) {
  const char *names[5] = {"NEGRO", "BLANCO", "ROJO", "VERDE", "AZUL"};
  uint16_t colors[5] = {ST77XX_BLACK, ST77XX_WHITE, ST77XX_RED, ST77XX_GREEN, ST77XX_BLUE};
  int k = step % 5;
  timedFill(colors[k]);
  printFillReport(names[k]);
}

void pattern1Start() {
  Serial.println("\n[1] COLORES SOLIDOS: negro, blanco, rojo, verde, azul (1.5 s cada uno)");
  printObserve("cada color cubre TODA la pantalla, uniforme, sin franjas ni parpadeo.");
  printObserve("ROJO debe verse rojo y AZUL azul (si estan intercambiados, use 'v').");
  printObserve("en BLANCO: brillo parejo; si se ve palido/inestable, revise la alimentacion.");
  pattern1Step(0);
}

// ---- Patrón 2: marco de calibración ----
void pattern2Start() {
  int w = tft.width(), h = tft.height();
  timedFill(ST77XX_BLACK);
  tft.drawRect(0, 0, w, h, ST77XX_WHITE);                 // marco de 1 px en el borde
  for (int x = 10; x < w; x += 10) {                       // reglas cada 10 px
    int len = (x % 50 == 0) ? 6 : 3;
    tft.drawFastVLine(x, 0, len, ST77XX_WHITE);
    tft.drawFastVLine(x, h - len, len, ST77XX_WHITE);
  }
  for (int y = 10; y < h; y += 10) {
    int len = (y % 50 == 0) ? 6 : 3;
    tft.drawFastHLine(0, y, len, ST77XX_WHITE);
    tft.drawFastHLine(w - len, y, len, ST77XX_WHITE);
  }
  tft.fillRect(0, 0, 4, 4, ST77XX_RED);                    // esquinas de colores
  tft.fillRect(w - 4, 0, 4, 4, ST77XX_GREEN);
  tft.fillRect(0, h - 4, 4, 4, ST77XX_BLUE);
  tft.fillRect(w - 4, h - 4, 4, 4, ST77XX_YELLOW);
  tft.drawFastHLine(w / 2 - 10, h / 2, 21, ST77XX_CYAN);   // cruz central
  tft.drawFastVLine(w / 2, h / 2 - 10, 21, ST77XX_CYAN);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(8, 8);   tft.print("R");
  tft.setCursor(w - 13, 8); tft.print("G");
  tft.setCursor(8, h - 15); tft.print("B");
  tft.setCursor(w - 13, h - 15); tft.print("Y");
  tft.setCursor(w / 2 - 27, h / 2 + 16);
  tft.printf("%d x %d", w, h);

  Serial.println("\n[2] MARCO DE CALIBRACION (resolucion y desplazamiento)");
  Serial.printf ("   La libreria reporta %d x %d px.\n", w, h);
  printObserve("un marco BLANCO de 1 px pegado a los 4 bordes, completo en los 4 lados.");
  printObserve("NO deben aparecer lineas o columnas de pixeles de colores fuera del marco.");
  printObserve("esquinas: ROJA arriba-izq, VERDE arriba-der, AZUL abajo-izq, AMARILLA abajo-der.");
  printObserve("si falta un lado o hay basura en 1-2 px del borde: desplazamiento -> use 'v'.");
}

// ---- Patrón 3: barras de color + degradado ----
void pattern3Start() {
  int w = tft.width(), h = tft.height();
  timedFill(ST77XX_BLACK);
  const char *labels[6] = {"R", "G", "B", "C", "M", "Y"};
  uint16_t colors[6] = {ST77XX_RED, ST77XX_GREEN, ST77XX_BLUE, ST77XX_CYAN, ST77XX_MAGENTA, ST77XX_YELLOW};
  int barH = (h * 2) / 3;
  int barW = w / 6;
  for (int i = 0; i < 6; i++) {
    tft.fillRect(i * barW, 12, barW, barH - 12, colors[i]);
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_BLACK);
    tft.setCursor(i * barW + barW / 2 - 5, 20);
    tft.print(labels[i]);
  }
  drawTitle("R G B C M Y");
  int gy = barH + 4;                                        // degradado de grises
  int gh = h - gy - 2;
  for (int x = 0; x < w; x++) {
    uint8_t v = (uint8_t)((x * 255) / (w - 1));
    tft.drawFastVLine(x, gy, gh, tft.color565(v, v, v));
  }
  Serial.println("\n[3] BARRAS DE COLOR (orden de color) + degradado de grises");
  printObserve("R=rojo, G=verde, B=azul, C=cian, M=magenta, Y=amarillo.");
  printObserve("si R se ve azul y B rojo: orden RGB/BGR incorrecto -> pruebe 'v'.");
  printObserve("degradado inferior: de negro a blanco, suave, sin saltos bruscos.");
  printObserve("'i' invierte los colores (debe cambiar todo; vuelva a 'i' para restaurar).");
}

// ---- Patrón 4: texto ----
void pattern4Start() {
  timedFill(ST77XX_BLACK);
  int y = 2;
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(2, y); tft.print("Tamano 1 (6x8 px)"); y += 10;
  tft.setCursor(2, y); tft.print("ABCDEFGHIJKLMNOPQRSTU"); y += 10;
  tft.setCursor(2, y); tft.print("abcdefghijklmnopqrstu"); y += 10;
  tft.setCursor(2, y); tft.print("0123456789 %.:-+/()"); y += 14;
  tft.setTextColor(ST77XX_YELLOW);
  tft.setTextSize(2);
  tft.setCursor(2, y); tft.print("Tamano 2"); y += 18;
  tft.setCursor(2, y); tft.print("0123456789"); y += 22;
  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(3);
  tft.setCursor(2, y); tft.print("T3 123"); y += 28;
  tft.setTextColor(ST77XX_GREEN, ST77XX_BLUE);              // texto con fondo
  tft.setTextSize(1);
  tft.setCursor(2, y); tft.print(" Texto con fondo ");
  Serial.println("\n[4] TEXTO en tamanos 1, 2 y 3");
  printObserve("todo el texto legible y nitido; ninguna letra cortada en el borde izquierdo.");
  printObserve("tamano 1 legible a distancia normal de lectura (lo usaremos para etiquetas).");
  printObserve("el texto con fondo azul no deja pixeles sueltos alrededor.");
}

// ---- Patrón 5: figuras geométricas ----
void pattern5Start() {
  int w = tft.width(), h = tft.height();
  timedFill(ST77XX_BLACK);
  tft.drawLine(0, 0, w - 1, h - 1, ST77XX_RED);             // diagonales
  tft.drawLine(w - 1, 0, 0, h - 1, ST77XX_GREEN);
  int r = ((w < h) ? w : h) / 2 - 8;
  tft.drawCircle(w / 2, h / 2, r, ST77XX_WHITE);            // círculo grande
  tft.fillCircle(w / 2, h / 2, 6, ST77XX_YELLOW);
  tft.drawRect(6, 6, 30, 20, ST77XX_CYAN);
  tft.fillRoundRect(w - 40, 6, 34, 20, 5, ST77XX_MAGENTA);
  tft.fillTriangle(10, h - 8, 30, h - 36, 50, h - 8, ST77XX_BLUE);
  tft.drawTriangle(w - 50, h - 8, w - 30, h - 36, w - 10, h - 8, ST77XX_ORANGE);
  Serial.println("\n[5] FIGURAS GEOMETRICAS");
  printObserve("el circulo blanco se ve REDONDO (no ovalado): confirma la proporcion 1:1.");
  printObserve("las diagonales roja y verde llegan EXACTAMENTE a las 4 esquinas.");
  printObserve("rectangulos, triangulos y circulo relleno completos y con bordes limpios.");
}

// ---- Patrón 6: rotaciones (cambia cada 3 s) ----
void pattern6Step(int step) {
  int rot = step % 4;
  tft.setRotation(rot);
  int w = tft.width(), h = tft.height();
  timedFill(ST77XX_BLACK);
  tft.drawRect(0, 0, w, h, ST77XX_WHITE);
  int cx = w / 2;
  tft.fillTriangle(cx, 14, cx - 14, 40, cx + 14, 40, ST77XX_GREEN);   // flecha "arriba"
  tft.fillRect(cx - 5, 40, 11, 30, ST77XX_GREEN);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(cx - 34, h / 2 + 14);
  tft.printf("ROT %d", rot);
  tft.setTextSize(1);
  tft.setCursor(cx - 27, h / 2 + 36);
  tft.printf("%d x %d", w, h);
  bool ok = (rot % 2 == 0) ? (w == 128 && h == 160) : (w == 160 && h == 128);
  Serial.printf("   Rotacion %d: %d x %d px -> %s\n", rot, w, h, ok ? "OK" : "DIFERENTE");
}

void pattern6Start() {
  Serial.println("\n[6] ROTACIONES 0, 1, 2, 3 (3 s cada una)");
  printObserve("la flecha verde apunta hacia ARRIBA del texto en cada rotacion.");
  printObserve("rotaciones 0 y 2: vertical (128x160); 1 y 3: horizontal (160x128).");
  printObserve("el marco blanco sigue completo en todas las rotaciones.");
  pattern6Step(0);
}

void pattern6End() {
  tft.setRotation(currentRotation);   // restaurar la rotación elegida
}

// ---- Patrón 7: animación (estabilidad) ----
void pattern7DrawStatic() {
  int w = tft.width(), h = tft.height();
  timedFill(ST77XX_BLACK);
  tft.drawRect(0, 12, w, h - 12, ST77XX_BLUE);
  drawTitle(stabilityLong ? "ESTABILIDAD 5 min" : "ANIMACION");
}

void pattern7Start() {
  stabilityStartMs = millis();
  stabilityFrames  = 0;
  framesAtLastBeat = 0;
  lastBeatMs       = stabilityStartMs;
  boxX = 10; boxY = 40; boxDx = 2; boxDy = 3; barX = 1;
  pattern7DrawStatic();
  Serial.printf("\n[7] %s\n", stabilityLong ? "PRUEBA DE ESTABILIDAD (5 minutos)" : "ANIMACION (estabilidad corta)");
  printObserve("cuadro amarillo rebotando y barra verde barriendo, movimiento fluido.");
  printObserve("sin parpadeos de toda la pantalla, sin rayas, sin congelamientos.");
  printObserve("el contador de cuadros aumenta; el ESP32 no se reinicia.");
}

void pattern7Frame(uint32_t now) {
  int w = tft.width(), h = tft.height();
  const int boxS = 12;
  int top = 13, bottom = h - 2, left = 1, right = w - 2;
  // Borra la posición anterior del cuadro y dibuja la nueva
  tft.fillRect(boxX, boxY, boxS, boxS, ST77XX_BLACK);
  boxX += boxDx; boxY += boxDy;
  if (boxX <= left || boxX + boxS >= right)  { boxDx = -boxDx; boxX += 2 * boxDx; }
  if (boxY <= top + 20 || boxY + boxS >= bottom) { boxDy = -boxDy; boxY += 2 * boxDy; }
  tft.fillRect(boxX, boxY, boxS, boxS, ST77XX_YELLOW);
  // Barra de barrido (como la futura gráfica PPG)
  tft.drawFastVLine(barX, top + 12, 6, ST77XX_BLACK);
  barX++;
  if (barX >= right) barX = left;
  tft.drawFastVLine(barX, top + 12, 6, ST77XX_GREEN);
  // Contador
  stabilityFrames++;
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(3, top + 2);
  tft.printf("cuad %lu  t %lus  ", (unsigned long)stabilityFrames,
             (unsigned long)((now - stabilityStartMs) / 1000UL));
}

// ---- Patrón 8: velocidad de dibujo parcial ----
uint32_t timeColumns(int n) {
  int h = tft.height();
  uint32_t t0 = micros();
  for (int i = 0; i < n; i++) {
    int x = 2 + (i % (tft.width() - 4));
    tft.drawFastVLine(x, h / 2, 60, ST77XX_BLACK);          // borrar columna
    tft.drawLine(x, h / 2 + 30, x, h / 2 + 30 - (i % 25), ST77XX_GREEN);  // segmento de onda
  }
  return (micros() - t0) / n;
}

uint32_t timeNumberUpdate(int n) {
  uint32_t t0 = micros();
  for (int i = 0; i < n; i++) {
    tft.fillRect(4, 20, 72, 24, ST77XX_BLACK);
    tft.setTextSize(3);
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(4, 20);
    tft.print(100 + (i % 50));
  }
  return (micros() - t0) / n;
}

void pattern8Start() {
  timedFill(ST77XX_BLACK);
  drawTitle("VELOCIDAD DE DIBUJO");
  float mhz = actualSpiMHz(SPI_SPEEDS_MHZ[speedIndex]);
  uint32_t fillUs = timedFill(ST77XX_BLACK);
  drawTitle("VELOCIDAD DE DIBUJO");
  uint32_t colUs  = timeColumns(200);
  uint32_t numUs  = timeNumberUpdate(50);
  uint32_t lineUs;
  {
    uint32_t t0 = micros();
    for (int i = 0; i < 100; i++) tft.drawLine(0, 50 + (i % 40), tft.width() - 1, 90 - (i % 40), ST77XX_CYAN);
    lineUs = (micros() - t0) / 100;
  }
  float typicalUpdateMs = (2 * numUs + 3 * colUs) / 1000.0f;   // 2 números + ~3 columnas por ciclo

  Serial.println("\n[8] VELOCIDAD DE DIBUJO PARCIAL (para la integracion con el MAX30102)");
  Serial.printf ("   SPI real ~%.2f MHz\n", mhz);
  Serial.printf ("   fillScreen completo        : %7.2f ms (NO usar en el ciclo de medicion)\n", fillUs / 1000.0f);
  Serial.printf ("   columna de onda (borrar+dibujar): %5lu us\n", (unsigned long)colUs);
  Serial.printf ("   actualizar un numero (tam. 3)   : %5lu us\n", (unsigned long)numUs);
  Serial.printf ("   linea diagonal                  : %5lu us\n", (unsigned long)lineUs);
  Serial.printf ("   Actualizacion tipica (2 numeros + 3 columnas): %.2f ms\n", typicalUpdateMs);
  Serial.printf ("   Margen del FIFO del MAX30102: %lu ms -> %s\n", (unsigned long)FIFO_BUDGET_MS,
                 (typicalUpdateMs < FIFO_BUDGET_MS / 10.0f) ? "AMPLIO (< 10 % del margen)" :
                 (typicalUpdateMs < FIFO_BUDGET_MS / 2.0f)  ? "SUFICIENTE" : "INSUFICIENTE: dibujar por partes");
  printObserve("la pantalla muestra numeros y lineas cambiando muy rapido (es normal).");
  printObserve("lo importante de este patron son los TIEMPOS del Monitor Serie.");
  tft.fillRect(0, 12, tft.width(), tft.height() - 12, ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(4, 20);  tft.printf("columna: %lu us", (unsigned long)colUs);
  tft.setCursor(4, 32);  tft.printf("numero : %lu us", (unsigned long)numUs);
  tft.setCursor(4, 44);  tft.printf("pantalla: %.1f ms", fillUs / 1000.0f);
  tft.setCursor(4, 60);  tft.print("Ver Monitor Serie");
}

// ---- Control de patrones ----
void startPattern(int n) {
  if (sweepActive) return;
  if (currentPattern == 6) pattern6End();
  if (n < 1 || n > NUM_PATTERNS) n = 1;
  currentPattern = n;
  patternStep    = 0;
  patternStartMs = millis();
  stepStartMs    = patternStartMs;
  if (n != 7) stabilityLong = false;
  switch (n) {
    case 1: pattern1Start(); break;
    case 2: pattern2Start(); break;
    case 3: pattern3Start(); break;
    case 4: pattern4Start(); break;
    case 5: pattern5Start(); break;
    case 6: pattern6Start(); break;
    case 7: pattern7Start(); break;
    case 8: pattern8Start(); break;
  }
}

void finishStabilityLong(uint32_t now) {
  float minutes = (now - stabilityStartMs) / 60000.0f;
  Serial.println("\n================================================");
  Serial.println(" RESULTADO DE LA PRUEBA DE ESTABILIDAD");
  Serial.println("================================================");
  Serial.printf (" Duracion: %.1f min | cuadros: %lu | promedio %.1f cuadros/s\n",
                 minutes, (unsigned long)stabilityFrames,
                 stabilityFrames / ((now - stabilityStartMs) / 1000.0f));
  Serial.println(" Sin reinicios del ESP32 durante la prueba (el programa siguio ejecutandose).");
  Serial.println(" Confirme visualmente: sin parpadeos, rayas ni congelamientos durante 5 min.");
  Serial.println("================================================\n");
  stabilityLong = false;
  startPattern(1);
}

void updatePattern(uint32_t now) {
  if (sweepActive) return;
  if (currentPattern == 1 && now - stepStartMs >= COLOR_STEP_MS) {
    patternStep++;
    stepStartMs = now;
    pattern1Step(patternStep);
  } else if (currentPattern == 6 && now - stepStartMs >= ROTATION_STEP_MS) {
    patternStep++;
    stepStartMs = now;
    pattern6Step(patternStep);
  } else if (currentPattern == 7) {
    if (now - lastFrameMs >= 20) {               // ~50 cuadros/s como máximo
      lastFrameMs = now;
      pattern7Frame(now);
    }
    if (now - lastBeatMs >= (stabilityLong ? 10000UL : 1000UL)) {
      uint32_t dt = now - lastBeatMs;
      Serial.printf("   [t=%lus] cuadros %lu (%.1f/s) | heap libre %lu bytes\n",
                    (unsigned long)((now - stabilityStartMs) / 1000UL), (unsigned long)stabilityFrames,
                    (stabilityFrames - framesAtLastBeat) * 1000.0f / dt, (unsigned long)ESP.getFreeHeap());
      lastBeatMs = now;
      framesAtLastBeat = stabilityFrames;
    }
    if (stabilityLong && now - stabilityStartMs >= STABILITY_LONG_MS) finishStabilityLong(now);
  }

  if (autoAdvance && !stabilityLong && now - patternStartMs >= AUTO_PATTERN_MS) {
    startPattern(currentPattern % NUM_PATTERNS + 1);
  }
}

// =================================================================
//  BARRIDO DE VELOCIDAD SPI
// =================================================================
void drawSweepPattern(int idx) {
  int w = tft.width(), h = tft.height();
  sweepFillUs[idx] = timedFill(ST77XX_BLACK);
  tft.drawRect(0, 0, w, h, ST77XX_WHITE);
  uint16_t colors[3] = {ST77XX_RED, ST77XX_GREEN, ST77XX_BLUE};
  for (int i = 0; i < 3; i++) tft.fillRect(4 + i * ((w - 8) / 3), 30, (w - 8) / 3, 40, colors[i]);
  for (int y = 76; y < h - 30; y += 2) tft.drawFastHLine(4, y, w - 8, (y % 4 == 0) ? ST77XX_WHITE : ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_YELLOW);
  tft.setCursor(6, 8);
  tft.printf("%lu MHz", (unsigned long)SPI_SPEEDS_MHZ[idx]);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(6, h - 20);
  tft.printf("real ~%.1f MHz", actualSpiMHz(SPI_SPEEDS_MHZ[idx]));
}

void sweepApplySpeed(int idx) {
#if !USE_SOFTWARE_SPI
  tft.setSPISpeed(SPI_SPEEDS_MHZ[idx] * 1000000UL);
#endif
  drawSweepPattern(idx);
  float mhz = actualSpiMHz(SPI_SPEEDS_MHZ[idx]);
  float theo = theoreticalFillMs(mhz);
  Serial.printf("   %2lu MHz (real ~%5.2f): fillScreen %6.1f ms, teorico %5.1f ms, relacion %.2f\n",
                (unsigned long)SPI_SPEEDS_MHZ[idx], mhz, sweepFillUs[idx] / 1000.0f, theo,
                (sweepFillUs[idx] / 1000.0f) / theo);
}

void startSweep() {
#if USE_SOFTWARE_SPI
  Serial.println(">> El barrido de velocidad no aplica con SPI por software.");
  return;
#endif
  if (currentPattern == 6) pattern6End();
  stabilityLong = false;
  sweepActive = true;
  sweepIndex  = 0;
  sweepStepMs = millis();
  Serial.println("\n[S] BARRIDO DE VELOCIDAD SPI (3 s por velocidad)");
  printObserve("en cada velocidad: marco completo, barras R G B limpias y rayas horizontales");
  printObserve("finas y regulares. Anote la velocidad MAS ALTA sin pixeles erroneos.");
  sweepApplySpeed(sweepIndex);
}

void updateSweep(uint32_t now) {
  if (!sweepActive || now - sweepStepMs < SWEEP_STEP_MS) return;
  sweepIndex++;
  sweepStepMs = now;
  if (sweepIndex < NUM_SPEEDS) {
    sweepApplySpeed(sweepIndex);
    return;
  }
  sweepActive = false;
#if !USE_SOFTWARE_SPI
  tft.setSPISpeed(requestedSpiHz());
#endif
  Serial.printf("   Fin del barrido. Velocidad restaurada a %lu MHz.\n", (unsigned long)SPI_SPEEDS_MHZ[speedIndex]);
  Serial.println("   Si alguna velocidad mostro errores, NO la use; elija con 'f' una inferior.\n");
  startPattern(currentPattern);
}

// =================================================================
//  INTERFAZ SERIE
// =================================================================
void printHelp() {
  Serial.println("\nComandos:");
  Serial.println("  n -> siguiente patron          1..8 -> ir a un patron");
  Serial.println("  a -> avance automatico (activar/desactivar, 10 s por patron)");
  Serial.println("  v -> alternar variante BLACKTAB / GREENTAB (reinicializa la pantalla)");
  Serial.println("  i -> invertir colores          r -> rotar (0-3)");
  Serial.println("  f -> cambiar velocidad SPI     s -> barrido de velocidades SPI");
  Serial.println("  e -> prueba de estabilidad de 5 minutos");
  Serial.println("  d -> diagnostico               h -> ayuda");
  Serial.println("Patrones: 1 colores | 2 marco | 3 barras | 4 texto | 5 figuras |");
  Serial.println("          6 rotaciones | 7 animacion | 8 velocidad de dibujo\n");
}

void printDiagnostics() {
  Serial.println("\n================ DIAGNOSTICO ================");
  Serial.printf (" Pines: SCK=%d MOSI=%d DC(A0)=%d RST=%d CS=%d (MISO %d reservado, sin conectar)\n",
                 TFT_SCK, TFT_MOSI, TFT_DC, TFT_RST, TFT_CS, TFT_MISO_UNUSED);
  Serial.printf (" Variante: %s | rotacion %d | invertido %s | %d x %d px\n",
                 variantName(currentVariant), currentRotation, inverted ? "SI" : "NO",
                 tft.width(), tft.height());
#if USE_SOFTWARE_SPI
  Serial.println(" SPI: por SOFTWARE");
#else
  Serial.printf (" SPI: hardware VSPI, %lu MHz solicitados (~%.2f MHz reales)\n",
                 (unsigned long)SPI_SPEEDS_MHZ[speedIndex], actualSpiMHz(SPI_SPEEDS_MHZ[speedIndex]));
#endif
  Serial.printf (" Inicializacion: %.1f ms | ultimo fillScreen: %.1f ms\n",
                 initTimeUs / 1000.0f, lastFillUs / 1000.0f);
  Serial.printf (" Patron actual: %d | avance automatico: %s\n", currentPattern, autoAdvance ? "SI" : "NO");
  Serial.printf (" Ultimo reinicio del ESP32: %s | heap libre %lu bytes\n",
                 resetReasonText((int)esp_reset_reason()), (unsigned long)ESP.getFreeHeap());
  Serial.println("=============================================\n");
}

void handleSerialCommands() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r' || c == '\n' || c == ' ') continue;
    if (sweepActive && c != 'h' && c != 'd') {
      Serial.println(">> Barrido en curso; espere a que termine.");
      continue;
    }
    if (c >= '1' && c <= '8') {
      startPattern(c - '0');
    } else if (c == 'n' || c == 'N') {
      startPattern(currentPattern % NUM_PATTERNS + 1);
    } else if (c == 'a' || c == 'A') {
      autoAdvance = !autoAdvance;
      patternStartMs = millis();
      Serial.printf(">> Avance automatico: %s\n", autoAdvance ? "ACTIVADO (10 s)" : "DESACTIVADO");
    } else if (c == 'v' || c == 'V') {
      uint8_t next = (currentVariant == INITR_BLACKTAB) ? INITR_GREENTAB : INITR_BLACKTAB;
      Serial.printf(">> Cambiando variante a %s...\n", variantName(next));
      if (currentPattern == 6) pattern6End();
      tftInit(next);
      startPattern(currentPattern);
    } else if (c == 'i' || c == 'I') {
      inverted = !inverted;
      tft.invertDisplay(inverted);
      Serial.printf(">> Colores invertidos: %s\n", inverted ? "SI" : "NO");
    } else if (c == 'r' || c == 'R') {
      if (currentPattern == 6) pattern6End();
      currentRotation = (currentRotation + 1) % 4;
      tft.setRotation(currentRotation);
      Serial.printf(">> Rotacion %d: %d x %d px\n", currentRotation, tft.width(), tft.height());
      startPattern(currentPattern == 6 ? 2 : currentPattern);
    } else if (c == 'f' || c == 'F') {
#if USE_SOFTWARE_SPI
      Serial.println(">> Con SPI por software la velocidad no es configurable.");
#else
      speedIndex = (speedIndex + 1) % NUM_SPEEDS;
      tft.setSPISpeed(requestedSpiHz());
      Serial.printf(">> Velocidad SPI: %lu MHz solicitados (~%.2f MHz reales)\n",
                    (unsigned long)SPI_SPEEDS_MHZ[speedIndex], actualSpiMHz(SPI_SPEEDS_MHZ[speedIndex]));
      startPattern(currentPattern);
#endif
    } else if (c == 's' || c == 'S') {
      startSweep();
    } else if (c == 'e' || c == 'E') {
      stabilityLong = true;
      autoAdvance   = false;
      if (currentPattern == 6) pattern6End();
      currentPattern = 7;
      patternStep    = 0;
      patternStartMs = millis();
      pattern7Start();
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
  Serial.println(" ETAPA 7.1 - Verificacion de la TFT ST7735 1.8\" 128x160");
  Serial.println(" Prototipo academico - sketch independiente (sin MAX30102)");
  Serial.println("================================================");
  Serial.printf (" Ultimo reinicio del ESP32: %s\n", resetReasonText((int)esp_reset_reason()));
  Serial.println(" Al encender: la retroiluminacion debe verse (pantalla iluminada");
  Serial.println(" aunque este en blanco o negro). Si no enciende: revise LED y VCC.");

  tftInit(VARIANT_DEFAULT);
  printHelp();
  Serial.println("Comenzando con el patron 1. Use 'n' para avanzar o 'a' para modo automatico.");
  startPattern(1);
}

void loop() {
  uint32_t now = millis();
  handleSerialCommands();
  updateSweep(now);
  updatePattern(now);
}
