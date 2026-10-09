/*
 * =====================================================================
 *  ETAPA 2 - Verificación de la comunicación I2C con el MAX30102
 * =====================================================================
 *  Proyecto : Monitor experimental de frecuencia cardiaca y SpO2
 *  Materia  : Sistemas de Información Hospitalaria
 *             Maestría en Ingeniería Clínica
 *  Placa    : NodeMCU ESP-32S (ESP32-WROOM-32)
 *  Entorno  : Arduino IDE, Arduino-ESP32 3.3.x (ESP-IDF 5.5)
 *  Librería : SparkFun MAX3010x Pulse and Proximity Sensor Library
 *             + Wire (incluida en el paquete ESP32)
 *
 *  PROTOTIPO ACADÉMICO Y EXPERIMENTAL.
 *  NO es un equipo médico certificado ni un dispositivo de diagnóstico.
 *
 *  Conexiones (SIN TFT en esta etapa):
 *    MAX30102 VIN -> 3V3      MAX30102 SDA -> GPIO 21
 *    MAX30102 GND -> GND      MAX30102 SCL -> GPIO 22
 *    INT y RD sin conectar
 *
 *  Qué hace:
 *    1. Revisa el nivel eléctrico de SDA/SCL antes de iniciar I2C.
 *    2. Inicia el bus I2C en GPIO21 (SDA) / GPIO22 (SCL).
 *    3. Escanea el bus y muestra las direcciones encontradas.
 *    4. Lee registros del MAX30102 directamente (byte a byte).
 *    5. Prueba escritura y relectura de un registro.
 *    6. Lee la temperatura interna del chip.
 *    7. Mide la confiabilidad del bus a 100 kHz y 400 kHz.
 *    8. Inicializa el sensor con la librería SparkFun.
 *    Después verifica la conexión cada 5 s.
 *
 *  NO se calcula BPM, SpO2 ni se procesa la señal PPG en esta etapa.
 *  Los LED del sensor NO se encienden en esta etapa.
 *
 *  Comandos por Monitor Serie (115200 baudios):
 *    t -> repetir todas las pruebas
 *    s -> solo escanear el bus I2C
 *    r -> mostrar tabla de registros
 *    h -> ayuda
 * =====================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include "MAX30105.h"   // Librería SparkFun (compatible con MAX30102)

// ------------------------- Configuración -------------------------
const int      PIN_SDA            = 21;
const int      PIN_SCL            = 22;
const uint32_t I2C_FREQ_STANDARD  = 100000;   // 100 kHz (modo estándar)
const uint32_t I2C_FREQ_FAST      = 400000;   // 400 kHz (modo rápido)
const uint16_t I2C_TIMEOUT_MS     = 50;
const uint32_t SERIAL_BAUD        = 115200;
const uint32_t MONITOR_PERIOD_MS  = 5000;     // verificación periódica
const int      RELIABILITY_READS  = 200;

// ------------------------- MAX30102 ------------------------------
const uint8_t MAX30102_ADDR       = 0x57;     // dirección I2C de 7 bits (fija)
const uint8_t EXPECTED_PART_ID    = 0x15;

// Mapa de registros (hoja de datos MAX30102)
const uint8_t REG_INT_STATUS_1    = 0x00;
const uint8_t REG_INT_STATUS_2    = 0x01;
const uint8_t REG_INT_ENABLE_1    = 0x02;
const uint8_t REG_INT_ENABLE_2    = 0x03;
const uint8_t REG_FIFO_WR_PTR     = 0x04;
const uint8_t REG_OVF_COUNTER     = 0x05;
const uint8_t REG_FIFO_RD_PTR     = 0x06;
const uint8_t REG_FIFO_CONFIG     = 0x08;
const uint8_t REG_MODE_CONFIG     = 0x09;
const uint8_t REG_SPO2_CONFIG     = 0x0A;
const uint8_t REG_LED1_PA         = 0x0C;     // corriente LED rojo
const uint8_t REG_LED2_PA         = 0x0D;     // corriente LED infrarrojo
const uint8_t REG_TEMP_INT        = 0x1F;
const uint8_t REG_TEMP_FRAC       = 0x20;
const uint8_t REG_TEMP_CONFIG     = 0x21;
const uint8_t REG_REV_ID          = 0xFE;
const uint8_t REG_PART_ID         = 0xFF;

MAX30105 particleSensor;   // Objeto de la librería SparkFun

// ------------------------- Resultados ----------------------------
struct TestResults {
  bool    linesHigh       = false;
  bool    busStarted      = false;
  int     devicesFound    = 0;
  bool    found0x57       = false;
  bool    partIdOk        = false;
  uint8_t partId          = 0;
  uint8_t revId           = 0;
  bool    writeReadOk     = false;
  bool    tempOk          = false;
  float   tempC           = NAN;
  int     errors100k      = -1;
  int     errors400k      = -1;
  float   usPerRead100k   = 0;
  float   usPerRead400k   = 0;
  bool    libraryOk       = false;
} results;

uint32_t lastMonitorMs  = 0;
uint32_t monitorCount   = 0;
uint32_t monitorErrors  = 0;

// =================================================================
//  Funciones de bajo nivel: acceso directo a registros por I2C
// =================================================================

// Lee un registro de 8 bits.
// Secuencia I2C: START | 0x57+W | registro | RESTART | 0x57+R | dato | STOP
bool readRegister(uint8_t reg, uint8_t &value) {
  Wire.beginTransmission(MAX30102_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;   // false = repeated start
  if (Wire.requestFrom(MAX30102_ADDR, (size_t)1) != 1) return false;
  if (!Wire.available()) return false;
  value = (uint8_t)Wire.read();
  return true;
}

// Escribe un registro de 8 bits.
// Secuencia I2C: START | 0x57+W | registro | dato | STOP
bool writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(MAX30102_ADDR);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

const char* i2cErrorText(uint8_t code) {
  switch (code) {
    case 0:  return "OK (ACK recibido)";
    case 1:  return "datos demasiado largos";
    case 2:  return "NACK en direccion (nadie respondio)";
    case 3:  return "NACK en datos";
    case 5:  return "tiempo agotado (bus bloqueado)";
    default: return "error desconocido del bus";
  }
}

void printHex(uint8_t v) {
  Serial.printf("0x%02X", v);
}

// =================================================================
//  PRUEBA 1: Nivel eléctrico de SDA y SCL antes de iniciar I2C
// =================================================================
// I2C usa salidas de "drenador abierto": los dispositivos solo pueden
// poner la línea en LOW; el nivel HIGH lo dan resistencias pull-up.
// Activamos una resistencia interna débil hacia GND (pull-down): si la
// línea aun así se lee HIGH, existe una pull-up externa (la del módulo).
void testLineLevels() {
  Serial.println("\n[1] Nivel de las lineas SDA/SCL (antes de iniciar I2C)");
  Wire.end();                        // libera los pines si el bus ya estaba activo
  pinMode(PIN_SDA, INPUT_PULLDOWN);
  pinMode(PIN_SCL, INPUT_PULLDOWN);
  delay(5);
  int sda = digitalRead(PIN_SDA);
  int scl = digitalRead(PIN_SCL);
  Serial.printf("    SDA (GPIO %d): %s\n", PIN_SDA, sda ? "HIGH" : "LOW");
  Serial.printf("    SCL (GPIO %d): %s\n", PIN_SCL, scl ? "HIGH" : "LOW");
  results.linesHigh = (sda == HIGH && scl == HIGH);
  if (results.linesHigh) {
    Serial.println("    -> OK: se detectan resistencias pull-up en el bus.");
  } else {
    Serial.println("    -> AVISO: alguna linea esta en LOW. Posibles causas:");
    Serial.println("       - Modulo sin alimentacion (revisar VIN y GND).");
    Serial.println("       - Cable de SDA o SCL suelto o en el pin equivocado.");
    Serial.println("       - Pull-ups del modulo conectadas a 1.8 V (algunos modulos genericos).");
    Serial.println("       Se continua con las pruebas para obtener mas informacion.");
  }
}

// =================================================================
//  PRUEBA 2: Inicio del bus I2C
// =================================================================
void startBus(uint32_t freq) {
  Wire.end();                                   // por si ya estaba iniciado
  results.busStarted = Wire.begin(PIN_SDA, PIN_SCL, freq);
  Wire.setTimeOut(I2C_TIMEOUT_MS);
}

void testBusStart() {
  Serial.println("\n[2] Inicio del bus I2C");
  startBus(I2C_FREQ_STANDARD);
  Serial.printf("    Wire.begin(SDA=%d, SCL=%d, %lu Hz): %s\n",
                PIN_SDA, PIN_SCL, (unsigned long)I2C_FREQ_STANDARD,
                results.busStarted ? "OK" : "FALLO");
  Serial.printf("    Frecuencia de reloj configurada: %lu Hz\n", (unsigned long)Wire.getClock());
}

// =================================================================
//  PRUEBA 3: Escaneo del bus I2C
// =================================================================
// Se "llama" a cada dirección posible (0x01-0x7E). Un dispositivo
// presente responde con ACK (código 0).
void scanBus() {
  Serial.println("\n[3] Escaneo del bus I2C (direcciones 0x01 a 0x7E)");
  results.devicesFound = 0;
  results.found0x57 = false;
  int busErrors = 0;

  for (uint8_t addr = 0x01; addr <= 0x7E; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
      results.devicesFound++;
      Serial.printf("    Dispositivo encontrado en 0x%02X", addr);
      if (addr == MAX30102_ADDR) {
        results.found0x57 = true;
        Serial.print("  <- MAX30102 (direccion esperada)");
      }
      Serial.println();
    } else if (err != 2) {
      busErrors++;
      if (busErrors <= 3) {
        Serial.printf("    Error en 0x%02X: codigo %u = %s\n", addr, err, i2cErrorText(err));
      }
    }
  }

  Serial.printf("    Total de dispositivos: %d\n", results.devicesFound);
  if (busErrors > 0) {
    Serial.printf("    Errores de bus durante el escaneo: %d\n", busErrors);
  }
  if (results.devicesFound == 0) {
    Serial.println("    -> Ningun dispositivo respondio.");
  } else if (!results.found0x57) {
    Serial.println("    -> Hay dispositivos, pero ninguno en 0x57 (no parece un MAX30102).");
  }
}

// =================================================================
//  PRUEBA 4: Lectura de registros de identificación y configuración
// =================================================================
void printRegister(const char* name, uint8_t reg, const char* note) {
  uint8_t v = 0;
  bool ok = readRegister(reg, v);
  Serial.printf("    %-14s (0x%02X): ", name, reg);
  if (ok) {
    printHex(v);
    Serial.printf("   %s\n", note);
  } else {
    Serial.println("ERROR DE LECTURA");
  }
}

void testIdentification() {
  Serial.println("\n[4] Registros de identificacion");
  uint8_t v = 0;

  if (readRegister(REG_PART_ID, v)) {
    results.partId = v;
    results.partIdOk = (v == EXPECTED_PART_ID);
    Serial.printf("    PART_ID  (0xFF): 0x%02X  (esperado 0x%02X) -> %s\n",
                  v, EXPECTED_PART_ID, results.partIdOk ? "OK" : "NO COINCIDE");
  } else {
    Serial.println("    PART_ID  (0xFF): ERROR DE LECTURA");
  }

  if (readRegister(REG_REV_ID, v)) {
    results.revId = v;
    Serial.printf("    REV_ID   (0xFE): 0x%02X  (revision del silicio, solo informativo)\n", v);
  } else {
    Serial.println("    REV_ID   (0xFE): ERROR DE LECTURA");
  }
}

void printRegisterTable() {
  Serial.println("\n    Tabla de registros del MAX30102");
  Serial.println("    (tras encender el sensor, la mayoria vale 0x00)");
  printRegister("INT_STATUS_1", REG_INT_STATUS_1, "estado de interrupciones 1 (se borra al leer)");
  printRegister("INT_STATUS_2", REG_INT_STATUS_2, "estado de interrupciones 2 (se borra al leer)");
  printRegister("INT_ENABLE_1", REG_INT_ENABLE_1, "habilitacion de interrupciones 1");
  printRegister("INT_ENABLE_2", REG_INT_ENABLE_2, "habilitacion de interrupciones 2");
  printRegister("FIFO_WR_PTR",  REG_FIFO_WR_PTR,  "puntero de escritura del FIFO");
  printRegister("OVF_COUNTER",  REG_OVF_COUNTER,  "muestras perdidas por desbordamiento");
  printRegister("FIFO_RD_PTR",  REG_FIFO_RD_PTR,  "puntero de lectura del FIFO");
  printRegister("FIFO_CONFIG",  REG_FIFO_CONFIG,  "promediado y comportamiento del FIFO");
  printRegister("MODE_CONFIG",  REG_MODE_CONFIG,  "modo de operacion (0x00 = sin medicion activa)");
  printRegister("SPO2_CONFIG",  REG_SPO2_CONFIG,  "rango ADC, muestras/s, ancho de pulso");
  printRegister("LED1_PA",      REG_LED1_PA,      "corriente del LED ROJO (0x00 = apagado)");
  printRegister("LED2_PA",      REG_LED2_PA,      "corriente del LED IR (0x00 = apagado)");
}

// =================================================================
//  PRUEBA 5: Escritura y relectura de un registro
// =================================================================
// Se usa INT_ENABLE_2 (0x03). Su único bit útil es el bit 1
// (DIE_TEMP_RDY_EN: avisar cuando termina la medición de temperatura).
// NO controla los LED, la corriente ni el modo de medición.
// Se invierte solo el bit 1, se relee y se restaura el valor original.
// El pin INT está desconectado, así que el cambio no tiene efecto externo.
void testWriteRead() {
  Serial.println("\n[5] Prueba de escritura y relectura (registro INT_ENABLE_2, no afecta los LED)");
  uint8_t original = 0, readBack = 0;

  if (!readRegister(REG_INT_ENABLE_2, original)) {
    Serial.println("    -> FALLO: no se pudo leer el valor original.");
    results.writeReadOk = false;
    return;
  }
  const uint8_t testValue = (uint8_t)((original ^ 0x02) & 0x02);  // solo el bit 1 es escribible
  bool wOk = writeRegister(REG_INT_ENABLE_2, testValue);
  bool rOk = readRegister(REG_INT_ENABLE_2, readBack);
  writeRegister(REG_INT_ENABLE_2, original);   // restaurar

  results.writeReadOk = wOk && rOk && (readBack == testValue);
  Serial.printf("    Escrito: 0x%02X  Leido: 0x%02X  Original restaurado: 0x%02X -> %s\n",
                testValue, readBack, original, results.writeReadOk ? "OK" : "FALLO");
}

// =================================================================
//  PRUEBA 6: Temperatura interna del chip
// =================================================================
// Procedimiento de la hoja de datos: escribir 1 en TEMP_EN (0x21),
// esperar a que el bit se borre (~29 ms) y leer parte entera (0x1F,
// complemento a dos) y fraccionaria (0x20, pasos de 0.0625 °C).
// Es la temperatura DEL CHIP, no la temperatura corporal.
void testTemperature() {
  Serial.println("\n[6] Temperatura interna del chip (NO es temperatura corporal)");
  results.tempOk = false;

  if (!writeRegister(REG_TEMP_CONFIG, 0x01)) {
    Serial.println("    -> FALLO: no se pudo iniciar la conversion.");
    return;
  }
  uint32_t t0 = millis();
  uint8_t cfg = 0x01;
  while ((cfg & 0x01) && (millis() - t0 < 100)) {
    delay(2);
    if (!readRegister(REG_TEMP_CONFIG, cfg)) break;
  }
  uint8_t tInt = 0, tFrac = 0;
  if ((cfg & 0x01) || !readRegister(REG_TEMP_INT, tInt) || !readRegister(REG_TEMP_FRAC, tFrac)) {
    Serial.println("    -> FALLO: la conversion no termino o no se pudo leer.");
    return;
  }
  results.tempC = (float)(int8_t)tInt + (tFrac & 0x0F) * 0.0625f;
  results.tempOk = (results.tempC > 0.0f && results.tempC < 60.0f);
  Serial.printf("    Temperatura: %.2f C (conversion en %lu ms) -> %s\n",
                results.tempC, (unsigned long)(millis() - t0),
                results.tempOk ? "OK (valor coherente)" : "valor fuera de rango");
}

// =================================================================
//  PRUEBA 7: Confiabilidad del bus a 100 kHz y 400 kHz
// =================================================================
// Se lee PART_ID muchas veces y se cuentan las lecturas fallidas o con
// valor incorrecto. En la Etapa 3 usaremos 400 kHz para leer el FIFO
// rápidamente, por eso es importante que esta prueba no tenga errores.
int reliabilityTest(uint32_t freq, float &usPerRead) {
  Wire.setClock(freq);
  int errors = 0;
  uint32_t t0 = micros();
  for (int i = 0; i < RELIABILITY_READS; i++) {
    uint8_t v = 0;
    if (!readRegister(REG_PART_ID, v) || v != EXPECTED_PART_ID) errors++;
  }
  usPerRead = (float)(micros() - t0) / RELIABILITY_READS;
  return errors;
}

void testReliability() {
  Serial.printf("\n[7] Confiabilidad del bus (%d lecturas de PART_ID por velocidad)\n",
                RELIABILITY_READS);
  results.errors100k = reliabilityTest(I2C_FREQ_STANDARD, results.usPerRead100k);
  Serial.printf("    100 kHz: %d errores, %.0f us por lectura\n",
                results.errors100k, results.usPerRead100k);
  results.errors400k = reliabilityTest(I2C_FREQ_FAST, results.usPerRead400k);
  Serial.printf("    400 kHz: %d errores, %.0f us por lectura\n",
                results.errors400k, results.usPerRead400k);
  Wire.setClock(I2C_FREQ_STANDARD);
}

// =================================================================
//  PRUEBA 8: Inicialización con la librería SparkFun
// =================================================================
// begin() verifica el PART_ID internamente. NO se llama a setup(),
// por lo que los LED del sensor permanecen apagados.
void testLibrary() {
  Serial.println("\n[8] Inicializacion con la libreria SparkFun MAX3010x");
  results.libraryOk = particleSensor.begin(Wire, I2C_SPEED_FAST);
  if (!results.libraryOk) {
    Serial.println("    particleSensor.begin(): FALLO (no reconocio el sensor)");
    Wire.setClock(I2C_FREQ_STANDARD);
    return;
  }
  Serial.println("    particleSensor.begin(): OK (bus a 400 kHz)");
  Serial.printf("    readPartID()      : 0x%02X\n", particleSensor.readPartID());
  Serial.printf("    getRevisionID()   : 0x%02X\n", particleSensor.getRevisionID());
  particleSensor.enableDIETEMPRDY();
  Serial.printf("    readTemperature() : %.2f C\n", particleSensor.readTemperature());
}

// =================================================================
//  Resumen y diagnóstico
// =================================================================
void printCheck(const char* name, bool ok) {
  Serial.printf("  [%s] %s\n", ok ? " OK " : "FALLA", name);
}

void printSummary() {
  bool reliable = (results.errors100k == 0 && results.errors400k == 0);
  bool passed = results.found0x57 && results.partIdOk && results.writeReadOk &&
                results.tempOk && reliable && results.libraryOk;

  Serial.println("\n================================================");
  Serial.println(" RESUMEN - ETAPA 2");
  Serial.println("================================================");
  printCheck("Lineas SDA/SCL en HIGH (pull-ups presentes)", results.linesHigh);
  printCheck("Bus I2C iniciado en GPIO21/GPIO22", results.busStarted);
  printCheck("Dispositivo detectado en 0x57", results.found0x57);
  printCheck("PART_ID = 0x15", results.partIdOk);
  printCheck("Escritura y relectura de registro", results.writeReadOk);
  printCheck("Temperatura interna coherente", results.tempOk);
  printCheck("0 errores a 100 kHz y 400 kHz", reliable);
  printCheck("Libreria SparkFun inicializada", results.libraryOk);
  Serial.println("------------------------------------------------");

  if (passed) {
    Serial.printf(" RESULTADO: MAX30102 DETECTADO CORRECTAMENTE en 0x%02X\n", MAX30102_ADDR);
    Serial.println(" ETAPA 2 SUPERADA");
    if (!results.linesHigh) {
      Serial.println(" Nota: la comunicacion funciona aunque las lineas se leyeron en LOW;");
      Serial.println(" probablemente las pull-ups del modulo estan a 1.8 V. Vigilar en etapas siguientes.");
    }
  } else {
    Serial.println(" RESULTADO: ETAPA 2 NO SUPERADA. Diagnostico sugerido:");
    if (!results.busStarted) {
      Serial.println("  - El bus no inicio: revisar version del paquete ESP32 y pines.");
    }
    if (results.devicesFound == 0) {
      Serial.println("  - Nadie responde: revisar VIN->3V3, GND->GND, SDA->21, SCL->22.");
      Serial.println("    Probar intercambiando SDA y SCL (es el error mas comun).");
      Serial.println("    Verificar con multimetro 3.3 V entre VIN y GND del modulo.");
    } else if (!results.found0x57) {
      Serial.println("  - Responde otro dispositivo, no el MAX30102: verificar el modulo.");
    }
    if (results.found0x57 && !results.partIdOk) {
      Serial.printf("  - PART_ID leido 0x%02X: posible modulo clon o comunicacion inestable.\n",
                    results.partId);
    }
    if (results.errors100k == 0 && results.errors400k > 0) {
      Serial.println("  - Falla solo a 400 kHz: acortar cables o agregar pull-ups de 4.7 kOhm a 3.3 V.");
    } else if (results.errors100k > 0) {
      Serial.println("  - Errores incluso a 100 kHz: falso contacto o pull-ups insuficientes.");
    }
  }
  Serial.println("================================================");
  Serial.println("Verificacion periodica cada 5 s. Comandos: t, s, r, h\n");
}

void runAllTests() {
  results = TestResults();
  testLineLevels();
  testBusStart();
  scanBus();
  if (results.found0x57) {
    testIdentification();
    printRegisterTable();
    testWriteRead();
    testTemperature();
    testReliability();
    testLibrary();
  } else {
    Serial.println("\n[4-8] Omitidas: el MAX30102 no se detecto en 0x57.");
  }
  printSummary();
  monitorCount = 0;
  monitorErrors = 0;
}

void printHelp() {
  Serial.println("\nComandos:");
  Serial.println("  t -> repetir todas las pruebas");
  Serial.println("  s -> escanear el bus I2C");
  Serial.println("  r -> tabla de registros del MAX30102");
  Serial.println("  h -> ayuda\n");
}

void handleSerialCommands() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r' || c == '\n' || c == ' ') continue;
    switch (c) {
      case 't': case 'T': runAllTests(); break;
      case 's': case 'S': scanBus(); break;
      case 'r': case 'R': printRegisterTable(); break;
      case 'h': case 'H': printHelp(); break;
      default:
        Serial.printf(">> Comando no reconocido: '%c' (escriba 'h')\n", c);
        break;
    }
  }
}

// -----------------------------------------------------------------
// Verificación periódica: detecta falsos contactos.
// Mueva suavemente los cables: si aparece un error, hay mala conexión.
// -----------------------------------------------------------------
void periodicCheck(uint32_t nowMs) {
  if (nowMs - lastMonitorMs < MONITOR_PERIOD_MS) return;
  lastMonitorMs = nowMs;
  monitorCount++;
  uint8_t id = 0;
  bool ok = readRegister(REG_PART_ID, id) && id == EXPECTED_PART_ID;
  if (!ok) monitorErrors++;
  Serial.printf("[t=%lu s] Verificacion #%lu: %s | errores acumulados: %lu\n",
                (unsigned long)(nowMs / 1000UL), (unsigned long)monitorCount,
                ok ? "MAX30102 responde (PART_ID 0x15)" : "SIN RESPUESTA",
                (unsigned long)monitorErrors);
}

// =================================================================
void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(1000);
  Serial.println();
  Serial.println("================================================");
  Serial.println(" ETAPA 2 - Verificacion del MAX30102 por I2C");
  Serial.println(" Prototipo academico - NO es dispositivo medico");
  Serial.println("================================================");
  Serial.printf(" SDA = GPIO %d | SCL = GPIO %d | Direccion esperada: 0x%02X\n",
                PIN_SDA, PIN_SCL, MAX30102_ADDR);

  runAllTests();
  lastMonitorMs = millis();
}

void loop() {
  handleSerialCommands();
  periodicCheck(millis());
}
