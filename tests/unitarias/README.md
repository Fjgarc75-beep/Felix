# Pruebas unitarias – Categoría 1 (PC)

Pruebas de las 45 funciones puras o deterministas de `Etapa9.ino`, ejecutadas en una PC. **Son simulación**: validan la lógica de cada función, no el comportamiento integrado ni el hardware.

Resultado de la última corrida (2026-10-09, desde esta carpeta del repositorio):

| Concepto | Resultado |
| --- | --- |
| Casos | 116 de 116 aprobados (113 principales + 3 de la variante `SPO2_DECIMALS = 0`) |
| Cobertura de líneas de las 45 funciones | 257 de 257 = 100 % (objetivo ≥ 85 %, no criterio de aprobación) |
| Cobertura de ramas (informativa) | 200 de 200 = 100 % |
| Prueba de mutación (informativa) | 16 de 16 errores introducidos detectados |
| MD5 del firmware antes y después | `4d70eb1aef89be2d7ab61ec6d39b9f12` |

Reporte completo: [REPORTE_Categoria1.md](REPORTE_Categoria1.md). Salidas de la corrida: [reports/](reports/).

## Cómo funciona

1. `run_tests.sh` verifica el MD5 y copia el `.ino` a `build/`.
2. Lo preprocesa con **arduino-cli** (el mismo preprocesador del IDE de Arduino) sobre una plataforma simulada (`harness/arduino/hardware/fake`).
3. El programa de pruebas `cat1/test_cat1.cpp` incluye el `.cpp` preprocesado; así accede a las funciones sin modificar el firmware.
4. Los valores esperados se calculan aparte, en Python con scipy (`reference/gen_reference_cat1.py`).
5. Se compila con `g++ --coverage`, se mide la cobertura con gcov y se corre la prueba de mutación sobre copias en `build/mut/`.

## Ejecutar

Requisitos: Linux x86_64 (o WSL en Windows), [arduino-cli](https://arduino.github.io/arduino-cli/) 1.x, g++ con gcov, git, y Python 3 con numpy y scipy.

```bash
cd tests/unitarias
./setup_deps.sh                                    # solo la primera vez
ARDUINO_CLI=/ruta/a/arduino-cli ./run_tests.sh cat1
```

`setup_deps.sh` descarga las dependencias de terceros, que no se versionan en este repositorio:

- SparkFun MAX3010x Sensor Library v1.1.2 (la misma del firmware);
- la fuente 5×7 de Adafruit GFX v1.12.6, para la pantalla simulada;
- `ctags` 5.8-arduino11, el que usa el preprocesador de Arduino (si ya está instalado con el IDE o arduino-cli, se copia de ahí; también se puede indicar con `CTAGS_BIN=/ruta/a/ctags`).

Códigos de salida de `run_tests.sh`: 0 todo aprobado; 1 casos fallidos; 2 falta una dependencia; 3 el MD5 no es el de la línea base; 4 falló el preprocesado; 5 el firmware cambió durante la corrida.

## Estructura

| Ruta | Contenido |
| --- | --- |
| `run_tests.sh` | Orquesta todo el proceso |
| `setup_deps.sh` | Descarga las dependencias de terceros |
| `cat1/test_cat1.cpp` | 113 casos + 3 de la variante |
| `cat1/coverage_cat1.py` | Cobertura de líneas y ramas de las 45 funciones |
| `cat1/mutation_cat1.py` | Prueba de mutación (16 mutantes) |
| `framework/mini_test.h` | Mini marco de pruebas con salida CSV, Markdown y JUnit |
| `reference/gen_reference_cat1.py` | Valores de referencia independientes (scipy) |
| `harness/` | Plataforma Arduino simulada: núcleo, `Wire` con simulador del MAX30102, SPI y TFT simuladas |
| `reports/` | Resultados de la última corrida |

## Limitaciones

- Se compila con g++ para x86-64, no con el compilador Xtensa del ESP32; los últimos bits de los cálculos en `float` pueden diferir, por eso se usan tolerancias explícitas.
- TFT, SPI y `Serial` son simulaciones.
- Son pruebas de caja blanca y no tienen valor clínico.
