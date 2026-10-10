# Resultados de pruebas

> Versión documentada: `firmware/etapa9-validada/Etapa9/Etapa9.ino`, MD5 `4d70eb1aef89be2d7ab61ec6d39b9f12`. Exportado al repositorio el 2026-10-09.

> **Origen y fuentes.** “Autor” es el autor del proyecto, que ejecutó y reportó las pruebas físicas. “Asistente” es el asistente de IA (Claude) que redactó el código y los documentos bajo la revisión y aprobación del autor. “Adjunto `xxxxxxxx`” identifica un reporte de resultados que el autor entregó durante el desarrollo; esos reportes son resúmenes y no incluyen el log completo.

Este documento separa **resultados físicos** (ESP32, MAX30102 y TFT reales, reportados por el autor), **resultados simulados en PC** y **pruebas pendientes de ejecución**. No se marca ninguna prueba como aprobada sin evidencia. La cobertura gcov ≥ 85 % es un **objetivo**, no un criterio de aprobación, salvo instrucción explícita posterior del profesor.

| Clase | Significado |
| --- | --- |
| A | Ejecutada y aprobada |
| B | Ejecutada con observaciones |
| C | Pendiente de ejecución |
| D | No aplicable o fuera de alcance |
| E | Resultado simulado en PC |
| F | Resultado obtenido físicamente sobre el ESP32 |

## 1. Resultados disponibles por clase

Hay resultados físicos de las etapas 4.1, 6.2, 7.1, 7.2 y 7.3, todos de etapas anteriores a la 9 y reportados por el autor. De la Etapa 9 no hay ningún resultado físico. El único resultado de la Etapa 10 es la Categoría 1, ejecutada en PC.

### F) Resultados obtenidos físicamente (ESP32 + MAX30102 + TFT reales)

| Etapa | Valores reportados | Fuente | Clase |
| --- | --- | --- | --- |
| 1, 2, 2.1, 3 | “Validada”, sin cifras. 2.1: “comunicación estable a 100 y 400 kHz” | Adjunto `6b7206cd` | A (aprobada, sin cifras) |
| 4.1 | AC RMS IR con dedo / fondo 55.4x; AC p-p IR medio 830.0; AC p-p RED medio 293.4; CV de amplitud IR 25 %; correlación AC RED-IR 0.98; recuperación DC IR 100 %; recuperación IR crudo 100 %; 0 muestras perdidas; 0 saturaciones; 99.1 muestras/s; 2 reinicios esperados (colocación y retiro); 0 reinicios por artefacto | `6b7206cd` | A, con observación I7 |
| 6.2 | 35 latidos válidos; R mediana ≈ 0.375; CV de R ≈ 4 %; correlación RED-IR 1.00; PI IR ≈ 0.87 %; SpO₂ experimental media ≈ 99.9 %; 0 pérdidas; 0 saturaciones; ≈ 99.1 muestras/s; “prueba de Etapa 6.2 superada” | `37499aa3` | B (aprobada con observación: SpO₂ ≥ 99 % sugiere sobreestimación de la curva; sin oxímetro de referencia) |
| 7.1 | 128×160; SPI 16 MHz; fillScreen ≈ 24.46 ms; columna de onda 142 µs; actualización de un número 3174 µs; actualización típica 6.77 ms; 5.0 min, 14998 cuadros, 50.0 cuadros/s, sin reinicios | `a39ddc66` | A |
| 7.2 | BPM ≈ 4.656 ms; SpO₂ ≈ 6.630 ms; calidad ≈ 6.902 ms; peor caso BPM+SpO₂+calidad ≈ 18.28 ms; heap libre ≈ 327 960 bytes (valores de BPM/SpO₂ simulados en pantalla) | `37499aa3` | A |
| 7.3 | Primera corrida con errores I2C (cantidad no disponible); corrida posterior correcta con 14 pérdidas y 1 desbordamiento acumulados; BPM+SpO₂+calidad ≈ 18.30 ms; FIFO ≈ 321 ms de capacidad | Resumen del historial y `7e7693dc` | B (aprobada con observaciones; origen de las 14 pérdidas sin determinar, H11) |
| 5, 6.1, 8.2 | Sin cifras físicas disponibles | — | Validación solo inferida |
| 9 | Ninguno | — | **C (pendiente de ejecución)** |

### E) Resultados simulados en PC

| Resultado | Valor | Fuente | Clase |
| --- | --- | --- | --- |
| Categoría 1, ejecución principal | 113 de 113 casos aprobados (45 normales, 31 límite, 37 error) | `tests/unitarias/reports/cat1_resultados.csv` | A (PC) |
| Categoría 1, variante `SPO2_DECIMALS = 0` | 3 de 3 casos aprobados | `cat1_variante_dec0_resultados.csv` | A (PC) |
| Cobertura de líneas de las 45 funciones | 257 de 257 = 100 % (objetivo ≥ 85 %, no criterio de aprobación) | `cat1_cobertura.md` | Informativo |
| Cobertura de ramas | 200 de 200 = 100 % | `cat1_cobertura.md` | Informativo |
| Prueba de mutación | 16 de 16 errores detectados | `cat1_mutacion.md` | Informativo |
| MD5 antes y después de la corrida | Igual a la línea base | `cat1_linea_base.txt` | A |
| Compilación de la Etapa 9 con el preprocesador real (arduino-cli 1.1.1) | Sin errores, 154 funciones | Arnés de PC | A (PC); no sustituye a H0 |
| Pantallas simuladas de las etapas 7.3, 8.2 y 9 | Imágenes PNG | Carpetas de cada etapa | Ilustrativo |
| Equivalencia de procesamiento con la 6.2 (citada en el plan, E17) | Declarada por el asistente en etapas anteriores | Plan de pruebas | Simulación; sin reporte archivado (I8) |

### D) No aplicables o fuera de alcance

- **Exactitud clínica de la SpO₂:** no hay oxímetro de referencia. La prueba guiada `w` lo imprime como “Concordancia con oximetro comercial: NO EVALUADA (sin equipo de referencia)” (L2659).
- **Pérdida de alimentación del sensor:** el plan la deja verificada solo en simulación (`POR_AT`); en hardware H10 solo desconecta SDA.



## 2. Resumen y detalle de resultados

Los resultados físicos corresponden a las etapas 4.1, 6.2, 7.1, 7.2 y 7.3 y los reportó el autor. De la Etapa 9 no hay resultados físicos. Los de PC están en una tabla aparte, para no mezclar simulación y hardware. Solo se marca como aprobada una prueba si existe evidencia; cuando un dato no se registró, la tabla lo dice.

### 2.1 Resumen de pruebas

| Prueba | Tipo | Resultado | Evidencia | Estado |
| --- | --- | --- | --- | --- |
| Etapas 1, 2, 2.1 y 3: tarjeta, I2C y FIFO | Hardware | Validadas por el autor; 2.1 “estable a 100 y 400 kHz” | Declaración del autor, sin cifras archivadas | Aprobada, sin cifras |
| Prueba guiada `t` (Etapa 4.1) | Hardware | 11 criterios cumplidos | Resultados reportados por el autor | Aprobada |
| Prueba guiada `w` (Etapa 6.2) | Hardware | Criterios internos cumplidos; SpO₂ media ≈ 99.9 % | Resultados reportados por el autor | Aprobada con observaciones |
| Verificación de la TFT (Etapa 7.1) | Hardware | 8 pruebas; 5 min estables | Resultados reportados por el autor | Aprobada |
| Interfaz con valores simulados (Etapa 7.2) | Hardware (TFT real) | Peor caso ≈ 18.28 ms | Resultados reportados por el autor | Aprobada |
| Integración sensor + TFT (Etapa 7.3) | Hardware | Errores I2C en la primera corrida; después correcta, con 14 pérdidas y 1 desbordamiento acumulados | Resumen del historial y adjunto `7e7693dc`; log original no archivado | Aprobada con observaciones |
| Gráfica PPG (Etapa 8.2) | Hardware | Sin cifras archivadas | — | Sin evidencia numérica |
| Etapa 9, H0–H13 | Hardware | — | — | Pendiente de ejecución |
| Categoría 1 (45 funciones) | Simulación en PC | 116 de 116 casos | [`tests/unitarias/reports/`](../../tests/unitarias/reports/) | Aprobada |
| Categorías 2a–2g y flujos E1–E17 | Simulación en PC | — | — | Pendiente de ejecución |

### 2.2 Resultados físicos en detalle

**Prueba de señal PPG `t`, Etapa 4.1.** Duración de la prueba: 31 s (10 s con el dedo quieto), según el diseño de `t`. Condición: dedo colocado y retirado según las indicaciones del Monitor Serie; corriente predeterminada de 6.2 mA (no se registró si se modificó).

| Medición | Valor medido | Unidad | Criterio | Resultado |
| --- | --- | --- | --- | --- |
| AC RMS IR con dedo / sin dedo | 55.4 | veces | ≥ 3 | Cumple |
| AC pico a pico IR medio | 830.0 | cuentas | ≥ 100 | Cumple |
| AC pico a pico RED medio | 293.4 | cuentas | ≥ 50 | Cumple |
| CV de la amplitud IR entre subventanas | 25 | % | ≤ 50 % | Cumple |
| Correlación AC RED–IR | 0.98 | — | ≥ 0.70 | Cumple |
| Recuperación del DC IR al retirar | 100 | % | ≥ 80 % | Cumple |
| Recuperación del IR crudo | 100 | % | Diagnóstico | — |
| Muestras perdidas | 0 | muestras | 0 | Cumple |
| Saturaciones | 0 | muestras | 0 | Cumple |
| Tasa de muestreo | 99.1 | muestras/s | 95–105 | Cumple |
| Reinicios de filtro | 2 esperados y 0 por artefacto | eventos | Observación | — |

El aumento del DC IR con dedo (criterio ≥ 10000 cuentas), el AC/DC IR (≥ 0.05 %) y el p-p mínimo por ventana (≥ 20) no se reportaron con su valor. El autor declaró aprobados los 11 criterios (I7).

**Prueba de SpO₂ `w`, Etapa 6.2.** Duración de la prueba: 56 s (30 s con el dedo quieto), según el diseño de `w`. Condición: dedo en reposo; sin oxímetro de referencia.

| Medición | Valor medido | Unidad | Criterio | Resultado |
| --- | --- | --- | --- | --- |
| Latidos válidos | 35 | latidos | ≥ 15 | Cumple |
| R mediana | ≈ 0.375 | — | Rango útil del firmware (no es criterio de w) | Dentro del rango útil |
| CV de R | ≈ 4 | % | ≤ 10 % | Cumple |
| Correlación RED–IR | 1.00 | — | ≥ 0.80 | Cumple |
| PI IR | ≈ 0.87 | % | ≥ 0.2 % | Cumple |
| SpO₂ experimental media | ≈ 99.9 | % | Plausible entre 88 y 100 % | Cumple el criterio interno; ver el análisis |
| Muestras perdidas y saturaciones | 0 y 0 | muestras | 0 | Cumple |
| Tasa de muestreo | ≈ 99.1 | muestras/s | 95–105 | Cumple |
| Concordancia con oxímetro comercial | — | — | — | NO EVALUADA |

**Pantalla, etapas 7.1 a 7.3.**

| Medición | Valor | Unidad | Condición o duración | Etapa |
| --- | --- | --- | --- | --- |
| Resolución | 128 × 160 | píxeles | `INITR_BLACKTAB` | 7.1 |
| Reloj SPI | 16 | MHz | — | 7.1 |
| `fillScreen` | ≈ 24.46 | ms | — | 7.1 |
| Columna de onda | 142 | µs | — | 7.1 |
| Actualización de un número | 3174 | µs | — | 7.1 |
| Actualización típica | 6.77 | ms | — | 7.1 |
| Estabilidad | 14998 cuadros, 50.0 cuadros/s, 0 reinicios | — | 5.0 min | 7.1 |
| Campo BPM / SpO₂ / calidad (promedio) | ≈ 4.656 / 6.630 / 6.902 | ms | Valores simulados en una TFT real | 7.2 |
| Peor caso BPM + SpO₂ + calidad | ≈ 18.28 | ms | Valores simulados | 7.2 |
| Memoria libre (heap) | ≈ 327960 | bytes | — | 7.2 |
| Peor caso BPM + SpO₂ + calidad | ≈ 18.30 | ms | Sensor y TFT integrados | 7.3 |
| Capacidad del FIFO | ≈ 321 | ms | 32 muestras / fs medida | 7.3 |
| Pérdidas y desbordamientos acumulados | 14 y 1 | muestras / eventos | Duración no registrada | 7.3 |



## 3. Pruebas unitarias en PC (Categoría 1)

**Resultado de la Categoría 1** (45 funciones puras o deterministas). Es un resultado en PC, es decir, de simulación:

| Concepto | Resultado |
| --- | --- |
| Casos (ejecución principal, `SPO2_DECIMALS = 1`) | 113 de 113 aprobados: 45 normales, 31 de límite, 37 de error |
| Casos (variante `SPO2_DECIMALS = 0`) | 3 de 3 aprobados |
| Verificaciones individuales | 4286 + 7 |
| Cobertura de líneas de las 45 funciones | 257 de 257 = 100 % (objetivo ≥ 85 %, **no** criterio de aprobación) |
| Cobertura de ramas | 200 de 200 = 100 % (informativa) |
| Líneas y ramas no cubiertas | 0 |
| Prueba de mutación | 16 de 16 errores introducidos fueron detectados (informativa) |
| MD5 antes y después | `4d70eb1aef89be2d7ab61ec6d39b9f12` en ambos casos |

El criterio de aprobación es doble: que pasen todos los casos y que el MD5 no cambie. La cobertura de gcov ≥ 85 % se documenta solo como **objetivo**; pasará a ser requisito únicamente si el profesor lo indica de forma explícita (F12).

Reporte completo: [`tests/unitarias/REPORTE_Categoria1.md`](../../tests/unitarias/REPORTE_Categoria1.md).

## 4. Pruebas de integración en PC

Están definidas en el plan y **pendientes de ejecución**. Usan el arnés de PC, que ya existe:

- núcleo simulado con reloj virtual;
- simulador del MAX30102 con FIFO, OVF, dedo programable, corte de bus (`DEAD_FROM`/`DEAD_TO`) y pérdida de configuración (`POR_AT`);
- TFT simulada sobre un framebuffer con la fuente real de GFX;
- inyector de comandos del Monitor Serie.

| Bloque | Qué verifica | Estado |
| --- | --- | --- |
| 2a (14 funciones) | I2C, FIFO, desbordamiento, fs medida, ventanas | Pendiente de ejecución |
| 2b (5) | DC, filtros, reinicios y estabilización contra Python | Pendiente de ejecución |
| 2c (10) | Detector de BPM de 40 a 180 BPM con muesca dicrótica y ruido | Pendiente de ejecución |
| 2d (15) | R con latidos sintéticos de R conocido; estados de SpO₂ | Pendiente de ejecución |
| 2e (25) | Pantalla píxel a píxel; actualización parcial; gráfica | Pendiente de ejecución |
| 2f (12) | P1–P4 con fallas inyectadas; 22 comandos | Pendiente de ejecución |
| 2g (26) | Pruebas guiadas `t`, `v`, `u`, `w` con escenarios de aprobación y de falla | Pendiente de ejecución |
| E1–E17 | setup() y loop() (Categoría 4, 2 funciones) en flujos de extremo a extremo: arranque, medición, retiro, movimiento, saturación, fallas del bus, sensor ausente, comandos, 24 h virtuales, equivalencia con la 6.2 | Pendiente de ejecución |



## 5. Pruebas sobre hardware (Etapa 9)

En etapas anteriores hubo pruebas físicas, ejecutadas por el autor con las pruebas guiadas del propio firmware y con mediciones de tiempos. Sus resultados se resumen en el capítulo 34. Para la versión documentada (Etapa 9), las 14 pruebas de hardware H0–H13 del plan están **pendientes de ejecución**:

| ID | Prueba | Procedimiento | Criterio | Estado |
| --- | --- | --- | --- | --- |
| H0 | Compilación con el núcleo real | Arduino IDE, Arduino-ESP32 3.3.x | Sin errores; memoria anotada | Pendiente de ejecución |
| H1 | Configuración del sensor | Comando `i` | PART\_ID 0x15; registros OK | Pendiente de ejecución |
| H2 | Adquisición sin pérdidas | `z`, 30 min con dedo, `y` cada 10 min | 0 errores I2C, 0 pérdidas de operación, fs 98–102 | Pendiente de ejecución |
| H3 | Señal óptica | Modo `k`, modo `d` con y sin dedo | Pulso visible; DC IR ≥ 50000 con dedo | Pendiente de ejecución |
| H4 | Corriente de LED | `+` y `-` con dedo | El DC sigue a la corriente | Pendiente de ejecución |
| H5 | BPM | Prueba `v` y conteo manual de 60 s | `v` aprobada; diferencia ≤ 5 BPM (referencia informal) | Pendiente de ejecución |
| H6 | SpO₂ experimental | Prueba `w`, tres veces | Criterios automáticos OK; concordancia NO EVALUADA | Pendiente de ejecución |
| H7 | Imagen de la pantalla | Fotos y video de cada estado | Distribución B, Ñ visible, sin parpadeo | Pendiente de ejecución |
| H8 | Barrido de la gráfica | Cronómetro | 5.1 ± 0.3 s por pantalla | Pendiente de ejecución |
| H9 | Interacción SPI–I2C | 10 min con gráfica y 10 min sin ella | Sin diferencia en errores ni pérdidas | Pendiente de ejecución |
| H10 | P1 y P2 | Desconectar SDA 2 s y 10 s | Reposición sola; recuperación 1/1 sin reiniciar el ESP32 | Pendiente de ejecución |
| H11 | Arranque (P3) | 5 encendidos | Origen de las 14 pérdidas identificado | Pendiente de ejecución |
| H12 | Alimentación y memoria | 60 min con dedo intermitente | Motivo de reinicio = encendido; memoria estable ±1 KB | Pendiente de ejecución |
| H13 | Funcionamiento prolongado | 60 min continuos | Sin reinicios ni valores congelados | Pendiente de ejecución |

La evidencia de cada prueba será la salida de `y`, los informes de las pruebas guiadas y fotos o video de la pantalla, registrados en el expediente de evidencias.



## 6. Pruebas pendientes

De las 154 funciones, 45 tienen pruebas ejecutadas (Categoría 1, en PC); las otras 109 y los 17 flujos están pendientes de ejecución, igual que las 14 pruebas de hardware. Por eso el proyecto **no puede declararse completamente validado**.

| Bloque del plan | Alcance | Entorno | Estado |
| --- | --- | --- | --- |
| Categoría 1 | 45 funciones puras o deterministas | PC | Ejecutada: 116 de 116 casos aprobados |
| Categoría 2a | 14 funciones de adquisición, I2C y FIFO | PC con simulador | Pendiente de ejecución |
| Categoría 2b | 5 funciones de procesamiento PPG | PC | Pendiente de ejecución |
| Categoría 2c | 10 funciones de BPM | PC | Pendiente de ejecución |
| Categoría 2d | 15 funciones de R y SpO₂ | PC | Pendiente de ejecución |
| Categoría 2e | 25 funciones de TFT y gráfica | PC con TFT simulada | Pendiente de ejecución |
| Categoría 2f | 12 funciones de P1–P4 y comandos | PC con simulador | Pendiente de ejecución |
| Categoría 2g | 26 funciones de pruebas guiadas | PC con guión de dedo | Pendiente de ejecución |
| Categoría 4 | `setup()`, `loop()` y flujos E1–E17 | PC y hardware | Pendiente de ejecución |
| Categoría 3 | H0–H13 | ESP32, MAX30102 y TFT reales | Pendiente de ejecución |

Pruebas de hardware que deciden la validación de la Etapa 9 (todas pendientes de ejecución):

| ID | Qué verifica | Criterio del plan |
| --- | --- | --- |
| H0 | Compilación con el núcleo real Arduino-ESP32 3.3.x | Sin errores; memoria anotada |
| H1 | Configuración del sensor (`i`) | PART\_ID 0x15; registros 0x50, 0x03, 0x2F y LED 0x1F en OK |
| H2 | Adquisición 30 min sin pérdidas | 0 errores I2C; 0 pérdidas de operación; fs 98–102; separación ≤ 35 ms |
| H3 | Señal óptica real | Pulso visible; DC IR ≥ 50 000 con dedo |
| H4 | Corriente de LED (`+`/`-`) | El DC sigue la corriente; mensajes de límite |
| H5 | BPM real (`v`) + conteo manual 60 s | `v` aprobada; diferencia ≤ 5 BPM (referencia informal) |
| H6 | SpO₂ experimental (`w` ×3) | Criterios automáticos OK; concordancia NO EVALUADA |
| H7 | Imagen de la pantalla | Distribución B; Ñ visible; sin parpadeo; aviso de 3 s |
| H8 | Barrido de la gráfica | 5.1 ± 0.3 s por pantalla |
| H9 | Interacción SPI–I2C | Sin diferencia con la gráfica activa o desactivada |
| H10 | P1/P2 desconectando SDA 2 s y 10 s | “--” en ≤ 1.5 s; recuperación P2 1/1 sin reinicio del ESP32 |
| H11 | Arranque P3 (5 encendidos) | Origen de las 14 pérdidas históricas identificado |
| H12 | Alimentación y memoria 60 min | Motivo de reinicio = encendido; memoria mínima estable (±1 KB) |
| H13 | Funcionamiento prolongado 60 min | Sin reinicios ni valores congelados |



## 7. Análisis de resultados

**Adquisición.** La tasa medida de 99.1 muestras/s, 0.9 % por debajo de la nominal, se repitió en las etapas 4.1 y 6.2. La causa más probable es la tolerancia del oscilador interno del sensor, aunque no se midió. Por eso el firmware convierte los intervalos con la fs medida y no con la nominal. Las 14 pérdidas de la 7.3 no se repitieron como dato en las etapas siguientes porque no hay cifras físicas de la 8.2 ni de la 9. Su origen, en el arranque o en operación, es justamente lo que P3 permite separar en la prueba H11, pendiente de ejecución.

**Señal PPG.** Una relación con dedo / sin dedo de 55.4 veces y una correlación RED–IR de 0.98 indican una señal de buena calidad en reposo. El CV de amplitud de 25 % está dentro del límite de 50 %, pero muestra que la amplitud del pulso varía aun con el dedo quieto.

**SpO₂.** Los criterios internos se cumplieron con margen: CV de R ≈ 4 % frente a un límite de 10 %, y correlación de 1.00. Sin embargo, una SpO₂ media de ≈ 99.9 % con R ≈ 0.375 sitúa al prototipo en la zona casi plana de la parábola, cerca del vértice (R ≈ 0.337). En esa zona, cambios grandes de R apenas cambian la SpO₂ calculada. El propio firmware advierte que una media ≥ 99 % “suele indicar que la curva sobreestima para este modulo” y que “se requiere calibracion”. **Este resultado no permite afirmar que la SpO₂ sea exacta**: solo muestra que el cálculo es estable y repetible con el dedo en reposo.

**Pantalla.** El peor caso de dibujo, ≈ 18.3 ms, es unas 17 veces menor que la capacidad del FIFO (≈ 320 ms). Por tiempo de dibujo, la pantalla no debería provocar desbordamientos del FIFO. Los errores I2C de la primera corrida de la 7.3 no se explican por ese tiempo, y su causa no quedó documentada; la prueba H9 (interacción SPI–I2C) está pendiente de ejecución.

**Verificación en PC.** La Categoría 1 confirma que las funciones de cálculo, como los filtros, la curva, las medianas, los umbrales de calidad y el formato, hacen lo que su especificación dice, con una referencia independiente. Las pruebas no validan el comportamiento integrado ni el físico.

**Estado global.** El proyecto **no está completamente validado**. La versión documentada (Etapa 9) tiene pendientes de ejecución 109 de 154 funciones en pruebas automatizadas, los 17 flujos y las 14 pruebas de hardware.
