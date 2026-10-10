# Historial de cambios

Cada etapa partió de la anterior. Las cifras de hardware son las que reportó el autor; las de PC son simulación. Las fechas son las de los archivos y reportes disponibles (fechas exactas de cada etapa pendientes: F8).

## Documentación – 2026-10-10

- Versiones en PDF (A4) de la documentación técnica y del manual de usuario en `docs/pdf/`, generadas a partir de los Markdown del repositorio.

## Documentación – 2026-10-09

- Publicación en el repositorio de la documentación técnica, el manual de usuario, la presentación (`.pptx` con notas y guion oral), el plan de pruebas, los resultados, la matriz de trazabilidad, el inventario documental, el expediente de evidencias, los diagramas y las pruebas unitarias de la Categoría 1.
- Corrección autorizada (inconsistencia I1): el plan de pruebas y el reporte de la Categoría 1 decían que `Etapa9.ino` tiene 4098 líneas; tiene 4112. El firmware no cambió.
- Arnés de pruebas: las dependencias de terceros (SparkFun MAX3010x, fuente de Adafruit GFX y `ctags`) ya no se versionan; las descarga `tests/unitarias/setup_deps.sh`. La ruta de volcado de la TFT simulada pasó a la variable `DUMP_DIR`.

## Etapa 10 – Pruebas (en curso)

- 2026-10-07: Categoría 1 ejecutada en PC: 116 de 116 casos, cobertura de líneas 257/257 y de ramas 200/200 (objetivo ≥ 85 %, no criterio de aprobación), mutación 16/16. MD5 del firmware sin cambios.
- Pendientes de ejecución: categorías 2a–2g, flujos E1–E17 y pruebas de hardware H0–H13.

## Etapa 9 – Robustez y diagnóstico (2026-10-02) · MD5 `4d70eb1aef89be2d7ab61ec6d39b9f12`

- P1: vigilancia de llegada de muestras; tras 1 s sin muestras, oculta BPM y SpO₂ y muestra “SENSOR NO RESPONDE”.
- P2: tras 3 s sin muestras, reinicializa el sensor por la ruta de reintento existente.
- P3: diagnóstico del arranque separado de las reinicializaciones posteriores (incluye la corrección autorizada `Etapa9_correccionP3.diff`).
- P4: sumas de tiempo de 64 bits, tramos de diagnóstico con `z` y `y` ampliado.
- Sin cambios en adquisición, PPG, BPM, R ni SpO₂. **Pendiente de validación en hardware.**

## Etapa 8.2 – Gráfica PPG (2026-10-02) · MD5 `f9c9c57b769c5505509531b88b9f9d4d`

- Gráfica PPG de barrido sobre el búfer existente, escala automática lenta y comando `l`. Sin cifras de hardware archivadas.

## Etapa 7.3 – Integración sensor + TFT (2026-10-01) · MD5 `c3bb33d0e216a8bcaa341d23ccbcf18f`

- La interfaz lee los estados existentes; actualización parcial cada 250/500 ms. Hardware: errores I²C en la primera corrida; después, correcta con 14 pérdidas y 1 desbordamiento acumulados; peor caso de dibujo ≈ 18.30 ms.

## Etapa 7.2 – Interfaz con datos simulados (2026-10-01) · MD5 `d0e9385c39682f96324fcdad56b46eeb`

- Interfaz de BPM, SpO₂, calidad y pie. Hardware (TFT real, valores simulados): peor caso ≈ 18.28 ms.

## Etapa 7.1 – Verificación de la TFT (2026-09-30) · MD5 `245bc2e6a458e25bf56b9b417d1cc1f8`

- 128 × 160, SPI 16 MHz, `INITR_BLACKTAB`. Hardware: 5.0 min estables, 14998 cuadros, columna de onda 142 µs.

## Etapa 6.2 – SpO₂ experimental (2026-09-30) · MD5 `ea2d0a065323784b63b60efae79229bc`

- Curva genérica R → SpO₂ y estados. Hardware: 35 latidos válidos, R ≈ 0.375, CV ≈ 4 %, SpO₂ media ≈ 99.9 % (con observaciones; sin oxímetro de referencia).

## Etapa 6.1 – R por latido (2026-09-30) · MD5 `5e5204aea757fc83c9e174b6c83d2200`

- R por regresión, pico a pico y RMS; PI, correlación y cambio de DC. Sin reporte propio.

## Etapa 5 – BPM (2026-09-30) · MD5 `9dd9f4e032f1dbf0a28432339c813e0c`

- Detector de pulsos y BPM con estados. Sin reporte propio.

## Etapa 4.1 – Procesamiento PPG (2026-09-29) · MD5 `0eb985309c7991450c377f8e88ce9ea2`

- Corrección de reinicios de filtros de la Etapa 4. Hardware (prueba `t`): 99.1 muestras/s, 0 pérdidas, 0 saturaciones, correlación RED–IR 0.98, relación con dedo / sin dedo 55.4.

## Etapas 1 a 4 (2026-09-28 a 2026-09-29)

- 1: ESP32 básico. 2 y 2.1: I²C con el MAX30102, estable a 100 y 400 kHz. 3: adquisición RED/IR por FIFO. 4: primer procesamiento PPG (sustituido por la 4.1). Validadas por el autor, sin cifras archivadas.
