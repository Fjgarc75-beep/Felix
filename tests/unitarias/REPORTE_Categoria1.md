# Etapa 10 – Reporte de pruebas, Categoría 1 (funciones puras / unitarias)

Firmware bajo prueba: `firmware/etapa9-validada/Etapa9/Etapa9.ino` (4112 líneas), **sin modificar**.

| Línea base | MD5 |
| --- | --- |
| Esperado | `4d70eb1aef89be2d7ab61ec6d39b9f12` |
| Antes de las pruebas | `4d70eb1aef89be2d7ab61ec6d39b9f12` |
| Después de las pruebas | `4d70eb1aef89be2d7ab61ec6d39b9f12` |

Tipo de resultado: **pruebas unitarias en PC (simulación)**. No sustituyen las pruebas físicas H0–H13 ni validan la Etapa 9 en hardware.

## 1. Resumen

| Concepto | Resultado |
| --- | --- |
| Funciones de la Categoría 1 probadas | 45 de 45 |
| Casos (ejecución principal, `SPO2_DECIMALS = 1`) | 113: 45 normales (N), 31 límite (L), 37 error (E) |
| Casos (variante `SPO2_DECIMALS = 0`) | 3 (L) |
| Total de casos | **116, aprobados 116, fallidos 0** |
| Verificaciones individuales | 4286 (+7 en la variante) |
| Cobertura de líneas, Categoría 1 | **257 de 257 = 100.0 %** (objetivo ≥ 85 %, no es criterio de aprobación) |
| Cobertura de ramas, Categoría 1 (informativa) | **200 de 200 = 100.0 %** |
| Líneas no cubiertas | **0** |
| Ramas no cubiertas | **0** |
| Prueba de mutación (informativa) | **16 de 16 errores introducidos detectados** |
| Cobertura del archivo completo (solo referencia) | 257 de 2329 = 11.0 % (las categorías 2a–2g aún no existen) |

Criterio de aprobación aplicado: todos los casos aprueban **y** el MD5 del firmware no cambia. La cobertura y la mutación se informan pero no deciden la aprobación.

## 2. Método

1. `run_tests.sh` verifica el MD5 y copia el `.ino` a `build/`.
2. El `.ino` se preprocesa con **arduino-cli 1.1.1** (el mismo preprocesador y ctags del Arduino IDE) sobre una plataforma simulada `fake:esp32:fake`. El `.cpp` resultante incluye los prototipos que genera el IDE.
3. El programa de pruebas `cat1/test_cat1.cpp` incluye ese `.cpp`. Así accede a funciones y variables globales sin tocar el firmware. Antes de cada caso, `antesDeCadaCaso()` deja el estado global en valores conocidos.
4. Valores de referencia **independientes del firmware**: `reference/gen_reference_cat1.py` usa Python/scipy (`butter`, `lfilter`, `freqz`) para los filtros y fórmulas exactas para la curva de SpO2 y las constantes exponenciales.
5. Compilación con `g++ -std=gnu++17 -O0 --coverage`. Cobertura con `gcov --json-format -b`, uniendo la ejecución principal y la variante de 0 decimales.
6. Variante `SPO2_DECIMALS = 0`: se genera con `sed` **sobre la copia en `build/`**, nunca sobre el `.ino`.
7. Prueba de mutación: se introduce un error en una copia del `.cpp` en `build/mut/<id>/` y se comprueba que algún caso falla.
8. Al final se verifica otra vez el MD5.

## 3. Resultados por función

N = normal, L = límite, E = error o entrada inválida. Todas las líneas y ramas de las 45 funciones están cubiertas al 100 %.

| # | Función | N | L | E | Verif. | Líneas | Ramas | Qué se comprueba (resumen) |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | `correlationFromSums` | 1 | 2 | 1 | 9 | 7 | 6 | Correlación +1/−1; varianza 0 en R o en IR; n = 0 o n = 1; sumas incoherentes sin NaN |
| 2 | `hrParabolicOffset` | 1 | 1 | 1 | 7 | 7 | 6 | Vértice exacto; recorte a ±0.5; denominador ≥ 0 da 0 |
| 3 | `spMedianOf` | 1 | 1 | 1 | 8 | 11 | 14 | Mediana; n = 1; tope de 64 valores (n = 65 usa los 64 primeros); n = 0 o negativo |
| 4 | `spCvOf` | 1 | 1 | 1 | 5 | 9 | 8 | CV muestral (n−1); n < 2; media ≤ 0 |
| 5 | `oxSpo2FromR` | 1 | 1 | 1 | 7 | 2 | 0 | Curva de Maxim contra la referencia (R = 0.428, 0.5, 0.337, 0.34, 1.15, 0) |
| 6 | `hrStateText` | 1 | 0 | 1 | 5 | 4 | 4 | Los 3 estados; código fuera de rango |
| 7 | `oxStateText` | 1 | 0 | 1 | 7 | 6 | 8 | Los 5 estados; código fuera de rango |
| 8 | `calidadTexto` | 1 | 1 | 1 | 12 | 7 | 5 | 5 categorías; longitud ≤ ancho del campo; código inválido |
| 9 | `calidadColor` | 1 | 0 | 1 | 7 | 6 | 4 | Color RGB565 por categoría; código inválido → gris claro |
| 10 | `modoTexto` | 1 | 1 | 1 | 8 | 4 | 4 | Los 3 textos del pie; caben en 21 caracteres; modo desconocido |
| 11 | `campoNombre` | 1 | 0 | 1 | 9 | 6 | 4 | Los 4 campos; índice inválido → "Pie" |
| 12 | `uiTextoCentrado` | 1 | 1 | 1 | 7 | 10 | 8 | Centrado; texto exacto o vacío; truncado sin escribir fuera del búfer (centinela) |
| 13 | `formatSpo2Key` | 1 | 1 | 1 | 10 | 6 | 4 | " 98.5", "100.0", "  0.0", " --.-"; búferes de 6 y 4 bytes sin desborde (centinela) |
| 14 | `uiSpo2FieldX` | 1 | 0 | 0 | 2 | 1 | 0 | Posición X con 1 decimal (y con 0 en la variante) |
| 15 | `uiPctX` | 1 | 0 | 0 | 1 | 1 | 0 | Posición del "%" (y con 0 decimales en la variante) |
| 16 | `resetReasonText` | 1 | 1 | 1 | 10 | 10 | 7 | Encendido, software, los 3 watchdog y brownout; motivo desconocido |
| 17 | `arranqueTipoTexto` | 1 | 0 | 1 | 4 | 4 | 4 | arranque, reintento, recuperación; tipo inválido → arranque |
| 18 | `isLargeStep` | 1 | 1 | 1 | 9 | 5 | 2 | Escalón relativo; mínimo absoluto; exactamente en el límite |
| 19 | `filtersSettled` | 1 | 1 | 1 | 3 | 2 | 4 | Combinaciones de cebado y asentamiento |
| 20 | `computeFilterCoefficients` | 1 | 1 | 1 | 18 | 22 | 0 | b/a contra scipy; −3.01 dB en fc; ganancia DC; alfas del DC (τ 1 s y 0.1 s); polos estables |
| 21 | `hrComputeConstants` | 1 | 0 | 0 | 1 | 3 | 0 | Caída de la envolvente contra exp(−1/(fs·τ)) |
| 22 | `highPass` | 1 | 1 | 1 | 2007 | 5 | 0 | Escalón y seno de 1.2 Hz contra `lfilter` (≤ 1e-4); canales independientes; entrada saturada (262143) sin NaN |
| 23 | `lowPass` | 1 | 1 | 1 | 2008 | 5 | 0 | Escalón, 1.2 Hz y 15 Hz contra `lfilter`; canales independientes; entrada saturada tiende a 262143 |
| 24 | `hrMeanInterval` | 1 | 1 | 1 | 4 | 5 | 4 | Media; 1 valor y búfer lleno (8); vacío |
| 25 | `hrMedianInterval` | 1 | 1 | 1 | 4 | 10 | 12 | Mediana impar; par (8 valores, promedio de los centrales); vacío |
| 26 | `hrComputeCv` | 1 | 1 | 1 | 3 | 8 | 4 | CV 0.2; 1 valor; vacío |
| 27 | `hrStableBpm` | 1 | 0 | 1 | 3 | 3 | 2 | 60 y 120 BPM; vacío |
| 28 | `hrRefractorySamples` | 1 | 2 | 0 | 7 | 6 | 4 | Mínimo de 300 ms contra 60 % de la media; fs 95 y 105 |
| 29 | `hrTimeoutSamples` | 1 | 1 | 0 | 5 | 6 | 4 | Mínimo de 3 s contra el factor por la media; fs 95 y 105 |
| 30 | `hrValleyBefore` | 1 | 2 | 1 | 10 | 14 | 16 | Ventana de 159; ventana de 1.1 ciclos; límite por `hrBufStartIdx`; pico < ventana; envoltura en 160 |
| 31 | `spMedianR` | 1 | 0 | 1 | 2 | 1 | 0 | Mediana del historial de R; vacío |
| 32 | `spCvR` | 1 | 0 | 1 | 2 | 1 | 0 | CV del historial de R; vacío |
| 33 | `testMeanDc` | 1 | 0 | 1 | 4 | 2 | 2 | Media por bloque y canal; sin muestras |
| 34 | `testMeanRaw` | 1 | 0 | 1 | 2 | 2 | 2 | Media cruda; sin muestras |
| 35 | `testAcRms` | 1 | 0 | 1 | 2 | 2 | 2 | RMS; sin muestras |
| 36 | `subMeanPP` | 1 | 1 | 1 | 3 | 5 | 4 | Media de las subventanas; 12 subventanas (máximo); ninguna |
| 37 | `subMinPP` | 1 | 0 | 1 | 2 | 5 | 6 | Mínimo; ninguna subventana |
| 38 | `subCvPP` | 1 | 1 | 1 | 3 | 9 | 6 | CV; menos de 2 subventanas; media ≤ 0 (devuelve 99) |
| 39 | `activeTestBlock` | 1 | 0 | 1 | 7 | 5 | 6 | Fase a bloque 0/1/2; fases sin medición dan −1 |
| 40 | `anyTestRunning` | 1 | 0 | 0 | 16 | 3 | 8 | Las 16 combinaciones de las 4 pruebas guiadas |
| 41 | `calidadDesdeEstados` | 1 | 1 | 1 | 12 | 9 | 12 | 6 filas validadas y 60 combinaciones contra tabla independiente; DC IR 50000/49999; sensor no listo |
| 42 | `oxQualityFailReason` | 1 | 1 | 1 | 17 | 7 | 10 | Cada motivo; umbrales exactos (en el límite y por debajo); prioridad entre motivos |
| 43 | `uiPpgValorAPixel` | 1 | 1 | 1 | 6 | 7 | 4 | Centro → fila 127; centro ± rango/2 → 108 y 145; fuera de escala recortado |
| 44 | `ppgMuestra` | 1 | 1 | 0 | 4 | 2 | 0 | −spAcIr[idx % 256]; 255, 256 y 2³²−1 (envoltura) |
| 45 | `ppgMuestraOk` | 1 | 1 | 0 | 4 | 2 | 0 | Bandera de validez; envoltura |

**Columnas L o E en 0.** No hay un caso del tipo correspondiente con sentido para esa función:

- `uiSpo2FieldX`, `uiPctX` y `hrComputeConstants` no reciben argumentos. Dependen solo de constantes, y su único límite (0 decimales) lo cubre la variante.
- `anyTestRunning` se prueba con las 16 combinaciones posibles.
- `ppgMuestra` y `ppgMuestraOk` aceptan cualquier índice `uint32_t`. Su único caso crítico (la envoltura) va como L.
- En las funciones de texto, el "límite" coincide con el código inválido (E).

El detalle de cada caso, con su descripción y número de verificaciones, está en `reports/cat1_resultados.md` y `.csv`. La versión JUnit está en `cat1_junit.xml`.

## 4. Cobertura

- **Líneas, Categoría 1:** 257 de 257 (100.0 %). Objetivo ≥ 85 %, cumplido. Es objetivo, no criterio de aprobación.
- **Ramas, Categoría 1:** 200 de 200 (100.0 %). Informativo; se excluyen las ramas de excepción que genera el compilador.
- **Líneas no cubiertas:** ninguna (`reports/cat1_lineas_no_cubiertas.csv` solo tiene el encabezado).
- **Ramas no cubiertas:** ninguna (`reports/cat1_ramas_no_cubiertas.csv` solo tiene el encabezado).
- La tabla por función, con el rango de líneas del `.ino`, está en `reports/cat1_cobertura.md` y `.csv`.

Las líneas que gcov informa se refieren al `.ino` original gracias a las directivas `#line` del preprocesador de Arduino.

**Nota de trazabilidad.** La primera medición de ramas dio 195 de 200. Las 5 ramas que faltaban tenían todas sus líneas cubiertas, pero no todos sus caminos. Para cubrirlas se agregaron 3 casos límite y una verificación:

| Caso o verificación | Rama que cubre |
| --- | --- |
| `C1_01_L2` | Varianza de IR igual a 0 con varianza de R mayor que 0 |
| `C1_28_L2` | Con historial, gana el mínimo de 300 ms |
| `C1_30_L2` | Historial largo que no amplía la ventana; inicio limitado por `hrBufStartIdx`; pico con índice menor que la ventana |
| Nueva verificación en `C1_01_E` | n = 1 con sumas incoherentes |

Ninguno de estos cambios afecta al firmware.

## 5. Prueba de mutación (informativa)

Sirve para comprobar que el 100 % de cobertura no es solo "líneas ejecutadas": si se introduce un error, las pruebas deben detectarlo. Se hace sobre copias en `build/mut/`, y el MD5 del `.ino` se verificó antes y después.

| Id | Función | Error introducido | Detectado por |
| --- | --- | --- | --- |
| M01 | `correlationFromSums` | `n < 2` → `n < 1` | C1_01_E |
| M02 | `isLargeStep` | `>` → `>=` | C1_18_L |
| M03 | `highPass` | Signo de a1 invertido | C1_22_N, C1_22_E |
| M04 | `hrMedianInterval` | Mediana par sin promediar | C1_25_L |
| M05 | `subMinPP` | Mínimo → máximo | C1_37_N |
| M06 | `calidadDesdeEstados` | OX estabilizando → REGULAR | C1_41_N, C1_41_L |
| M07 | `oxQualityFailReason` | Umbral PI `<` → `<=` | C1_42_L |
| M08 | `uiPpgValorAPixel` | Eje vertical invertido | C1_43_L, C1_43_E |
| M09 | `spCvOf` | Desviación con n en vez de n−1 | C1_04_N, C1_04_L |
| M10 | `uiTextoCentrado` | Sin truncar (desborda el búfer) | C1_12_E |
| M11 | `formatSpo2Key` | Decimal mal calculado | C1_13_N, C1_13_E |
| M12 | `activeTestBlock` | Bloque 2 devuelve 1 | C1_39_N |
| M13 | `oxSpo2FromR` | Coeficiente a: −45.060 → −45.600 | C1_05_N, C1_05_L |
| M14 | `hrValleyBefore` | Sin límite por `hrBufStartIdx` | C1_30_L2 |
| M15 | `hrRefractorySamples` | Fracción adaptativa no aplicada | C1_28_N |
| M16 | `testAcRms` | RMS sin raíz cuadrada | C1_35_N |

Resultado: **16 de 16 eliminados**.

En la primera corrida sobrevivieron dos. Se corrigieron así:

- **M01:** se agregó la verificación con n = 1 y sumas incoherentes.
- **M10:** la mutación original (`n > ancho + 1`) resultó **equivalente**: produce exactamente la misma salida. Se sustituyó por una que elimina el truncado.

## 6. Hallazgos (documentados, firmware sin cambios)

1. **Respuesta distinta ante media ≤ 0.** `spCvOf` devuelve 0 cuando la media es ≤ 0, mientras que `subCvPP` devuelve 99. Las dos conductas son seguras en su contexto: `spCvOf` se usa con R positivos, y en `subCvPP` el 99 fuerza "inestable". Aun así, es una diferencia de criterio que conviene anotar en la documentación técnica.
2. **"FUERA DE RANGO" por mediana de R inalcanzable.** Por el flujo normal no se puede llegar a "FUERA DE RANGO" a partir de la mediana de R: cada R se valida antes de entrar al historial. Se trata como caso de caja blanca, como ya indica el plan.
3. **Ventana de `hrValleyBefore`.** El búfer de 160 casillas solo permite comprobar la ventana de 159 desde dentro: un índice anterior caería en la misma casilla que el pico. Es una limitación de la prueba, no un defecto. Está comentada en `C1_30_L`.

## 7. Limitaciones

- Las pruebas se ejecutan en PC (x86-64, g++ con `-O0`), no en el ESP32 (Xtensa, `-Os`). Ambos usan `float` IEEE-754 de precisión simple, pero el orden de las operaciones y las optimizaciones pueden cambiar los últimos bits. Por eso las comparaciones de punto flotante usan tolerancias explícitas.
- TFT, SPI y `Serial` son simulaciones. La biblioteca SparkFun MAX3010x es la real, pero se ejecuta sobre un `Wire` simulado. La Categoría 1 no depende de esas simulaciones salvo para compilar.
- Son pruebas de caja blanca: fijan variables globales directamente. Validan la lógica de cada función, no el comportamiento integrado (categorías 2a–2g y flujos E1–E17), ni el hardware (H0–H13).
- Ningún resultado tiene valor clínico.

## 8. Cómo ejecutar

```bash
cd tests/unitarias
./setup_deps.sh                                   # solo la primera vez
ARDUINO_CLI=/ruta/a/arduino-cli ./run_tests.sh cat1
```

Requisitos: Linux x86_64 (o WSL), arduino-cli 1.x, g++ con gcov, git y Python 3 con numpy y scipy.

Códigos de salida:

| Código | Significado |
| --- | --- |
| 0 | Todo aprobado |
| 2 | Falta una dependencia (ejecute `./setup_deps.sh`) |
| 1 | Hay casos fallidos |
| 3 | El MD5 no coincide con la línea base |
| 4 | Falló el preprocesado |
| 5 | El firmware cambió durante la corrida |

Los reportes se generan en `reports/`.
