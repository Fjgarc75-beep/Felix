# Inventario documental – Monitor PPG (Etapa 9)

> Inventario documental de la línea base `Etapa9.ino` (MD5 `4d70eb1aef89be2d7ab61ec6d39b9f12`). Actualizado el 2026-10-09.

> **Origen y fuentes.** “Autor” es el autor del proyecto, que ejecutó y reportó las pruebas físicas. “Asistente” es el asistente de IA (Claude) que redactó el código y los documentos bajo la revisión y aprobación del autor. “Adjunto `xxxxxxxx`” identifica un reporte de resultados que el autor entregó durante el desarrollo; esos reportes son resúmenes y no incluyen el log completo.

La línea base es `Etapa9.ino` con MD5 `4d70eb1aef89be2d7ab61ec6d39b9f12`, verificado hoy y sin modificar. La Etapa 9 **no está validada en hardware**: sus pruebas físicas (H0–H13) están pendientes de ejecución, y lo único ejecutado de la Etapa 10 es la Categoría 1, en PC.

## Archivos revisados

Se revisaron todos los archivos y grupos de la tabla. Solo cuentan como evidencia de hardware los datos que el autor reportó desde el equipo real; lo que generó el asistente en PC es simulación.

| Archivo o grupo | Origen | Qué aporta | ¿Evidencia de hardware? |
| --- | --- | --- | --- |
| `Etapa9/Etapa9.ino` (4112 líneas, 154 funciones) | Firmware de línea base | Fuente única de funciones, parámetros y textos | No; es la especificación |
| `Etapa9/Etapa9_vs_Etapa8_2.diff`, `Etapa9_correccionP3.diff` | Asistente | Cambios P1–P4 y corrección de P3 | No |
| `Etapa9/simulacion_falla_sensor_9.png` | Asistente, PC | Pantalla simulada de P1/P2 | No (simulación) |
| Firmwares de las etapas 1 a 8.2 (13 archivos `.ino`) | Asistente, aprobados por el autor | Historia del diseño | No |
| `Etapa4_1_cambios.txt` | Asistente | Corrección de reinicios de filtros de la 4.1 | No |
| `Etapa7_3/simulacion_pantallas_7_3.png`, `Etapa8_2/simulacion_pantallas_8_2.png` | Asistente, PC | Pantallas simuladas | No (simulación) |
| Adjunto `6b7206cd` (29-sep-2026) | Autor | Resultados de la Etapa 4.1 y configuración validada de las etapas 1–3 | **Sí** |
| Adjunto `a39ddc66` (01-oct-2026) | Autor | Resultados de la Etapa 7.1 y datos de la TFT | **Sí** |
| Adjunto `37499aa3` (01-oct-2026) | Autor | Resultados de las etapas 6.2 y 7.2 | **Sí** |
| Adjunto `7e7693dc` (01-oct-2026) | Autor | Etapa 7.3 validada; 18.30 ms; FIFO ≈ 321 ms | **Sí** |
| Adjunto `5334e1dc` (07-oct-2026) | Autor | Esta solicitud; conexiones y condensador de 470 µF / 25 V | Solo como declaración de hardware |
| Resumen del historial de la conversación | Asistente (paráfrasis) | Episodio de errores I2C de la 7.3 y “14 perdidas / 1 desbordamiento” | Parcial: sin el log original |
| Plan de pruebas – Etapa 10 (Claude Doc, rev. 19) | Asistente, aprobado por el autor | Estructura de pruebas: categorías 1, 2a–2g, 3 (H0–H13), 4 (E1–E17) | No (plan) |
| `tests/unitarias/` (arnés, `run_tests.sh`, `reports/`, `REPORTE_Categoria1.md`) | Asistente, PC | Resultados de la Categoría 1 | No (simulación en PC) |
| `monitor_ppg_spo2.ino`, `servidor.py`, `schema.sql`, `monitor.db`, `monitor_ppg_corregido.ino`, adjunto `cb965922` | Enfoque inicial, anterior a la Etapa 1 | WiFi, servidor Flask, SQLite, FHIR, NeoPixel | No; **no forman parte del prototipo** |

El enfoque inicial con WiFi, servidor y base de datos se abandonó. El autor pidió expresamente no usar WiFi, Bluetooth, registro ni comunicación web, y la cabecera del firmware lo confirma: “NO se implementa: WiFi, Bluetooth, alarmas, microSD ni registro” (L116).

## Firmware de línea base

`Etapa9.ino` es la línea base: su MD5 calculado hoy es `4d70eb1aef89be2d7ab61ec6d39b9f12`, igual al indicado, y el archivo no se modificó. Tiene **4112 líneas** (el plan y el reporte de la Categoría 1 decían 4098; corregido al publicar, ver inconsistencia I1) y 154 funciones, incluidas `setup()` y `loop()`.

| Etapa | Archivo | MD5 | Estado declarado |
| --- | --- | --- | --- |
| 9 (línea base) | `Etapa9/Etapa9.ino` | `4d70eb1aef89be2d7ab61ec6d39b9f12` | Compilada con el preprocesador real de Arduino en PC; **pendiente de validación en hardware** |
| 8.2 | `Etapa8_2/Etapa8_2.ino` | `f9c9c57b769c5505509531b88b9f9d4d` | Validada según la cabecera de la Etapa 9; sin cifras de hardware disponibles |
| 7.3 | `Etapa7_3/Etapa7_3.ino` | `c3bb33d0e216a8bcaa341d23ccbcf18f` | Validada por el autor |
| 7.2 | `Etapa7_2_TFT/Etapa7_2_TFT.ino` | `d0e9385c39682f96324fcdad56b46eeb` | Validada por el autor (valores simulados en pantalla) |
| 7.1 | `Etapa7_1_TFT/Etapa7_1_TFT.ino` | `245bc2e6a458e25bf56b9b417d1cc1f8` | Validada por el autor |
| 6.2 | `Etapa6_2_SpO2/Etapa6_2_SpO2.ino` | `ea2d0a065323784b63b60efae79229bc` | Validada por el autor |
| 6.1 | `Etapa6_1_R/Etapa6_1_R.ino` | `5e5204aea757fc83c9e174b6c83d2200` | Sin reporte propio; incluida en la 6.2 validada |
| 5 | `Etapa5_BPM/Etapa5_BPM.ino` | `9dd9f4e032f1dbf0a28432339c813e0c` | Sin reporte propio; incluida en la 6.2 validada |
| 4.1 | `Etapa4_1_PPG/Etapa4_1_PPG.ino` | `0eb985309c7991450c377f8e88ce9ea2` | Validada por el autor |
| 4 | `Etapa4_PPG/Etapa4_PPG.ino` | `35bca706ee1a85f61dab3f42cda1befc` | Sustituida por la 4.1 |
| 3 | `Etapa3_RED_IR/Etapa3_RED_IR.ino` | `189e30e5b7aaf5d6f40cfd3053316a88` | Validada por el autor, sin cifras |
| 2.1 | `Etapa2_1_I2C_MAX30102/Etapa2_1_I2C_MAX30102.ino` | `e490a7cba7b88158deda5dbfa258af64` | Validada por el autor, sin cifras |
| 2 | `Etapa2_I2C_MAX30102/Etapa2_I2C_MAX30102.ino` | `d1de203df1fa0cf14581092de80fd534` | Validada por el autor, sin cifras |
| 1 | `Etapa1_ESP32_basico/Etapa1_ESP32_basico.ino` | `7f838ba17bc0453d56a3eaf20b51148c` | Validada por el autor, sin cifras |

La cabecera de la línea base dice que la Etapa 9 solo agrega vigilancia, recuperación y diagnóstico, y que “los algoritmos de adquisición, PPG, BPM, R y SpO2 NO cambian” (L6–7). El diff contra la 8.2 lo confirma: no toca esos módulos.

## Resultados disponibles

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

## Pruebas pendientes

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

## Matriz de trazabilidad

Cada capacidad que la documentación atribuye al sistema está ligada a su código en `Etapa9.ino` (número de línea), al origen de la decisión, a la prueba que la verifica y a la evidencia que existe hoy. “Lxxx” indica la línea del firmware.

| Capacidad | Implementación en el firmware | Origen de la decisión | Pruebas | Evidencia actual | Estado |
| --- | --- | --- | --- | --- | --- |
| I2C a 400 kHz, SDA 21, SCL 22 | `I2C_FREQ` L156; `initSensor` L510–531 | Autor (Etapa 2.1, validada) | 2a, H1, H2 | Etapa 2.1 “estable a 100 y 400 kHz” (sin cifras) | Hardware previo; Etapa 9 pendiente |
| Configuración del MAX30102: RED+IR, 400 sps, promedio 4, 411 µs, 4096 nA, 6.2 mA | L173–186; `particleSensor.setup` L526–527 | Autor (Etapa 3, validada) | 2a, H1 | Etapa 4.1: 99.1 muestras/s | Hardware previo; Etapa 9 pendiente |
| Lectura del FIFO por sondeo cada 10 ms, ráfagas de 10 muestras, OVF | `readFifo` L2771–2809 | Asistente, validado en la Etapa 3 | 2a, H2 | 4.1: 0 pérdidas; 7.3: 14 pérdidas / 1 desbordamiento | Con observación (H11 pendiente) |
| DC (τ 1 s), pasa-altas 0.5 Hz, pasa-bajas 5 Hz, estabilización de 2 s | `computeFilterCoefficients` L546; `processPPG` L669–709 | Autor (filtros) y asistente (4.1) | Cat. 1, 2b, H3 | Cat. 1: coeficientes = SciPy; 4.1 en hardware | Unitario OK (PC); hardware previo |
| Detección de pulsos y BPM (40–180, 30 % de la mediana, 8 intervalos) | `updateHeartRate` L1425; `hrEvaluateInterval` L1312; `hrUpdateState` L1360 | Requisitos del autor; algoritmo del asistente (Etapa 5) | Cat. 1 (parcial), 2c, H5 | Solo inferida por la 6.2 validada; sin cifras de BPM | Pendiente (H5) |
| R por latido (regresión, pico a pico, RMS), PI, correlación, ΔDC | `spo2OnBeat` L1980–2056 | Asistente (Etapa 6.1) | 2d, H6 | 6.2: R ≈ 0.375, CV ≈ 4 %, corr 1.00, PI ≈ 0.87 % | Hardware previo; Etapa 9 pendiente |
| SpO₂ experimental con curva genérica de Maxim | `oxSpo2FromR` L2361; `oxUpdateState` L2445; coeficientes L2271–2273 | Asistente (Etapa 6.2), aprobado al validar | Cat. 1, 2d, H6 | Cat. 1: curva = referencia Python; 6.2: media ≈ 99.9 % | **Sin validación clínica** |
| Calidad de señal en 5 categorías | `calidadDesdeEstados` L3542–3551 | Autor (Etapa 7.2/7.3) | Cat. 1 (60 combinaciones), H7 | Cat. 1 OK (PC) | Unitario OK; hardware pendiente |
| Interfaz TFT, distribución B, actualización parcial | `uiInit` L3198; `uiActualizarDesdeEstados` L3556 | Autor (7.1–7.3, 8.2) | 2e, H7 | 7.1/7.2/7.3 en hardware | Hardware previo; Etapa 9 pendiente |
| Gráfica PPG de barrido (4 muestras por columna, escala automática) | `uiPpg*` L3257–3528 | Autor (8.1) | 2e, H8, H9 | Sin cifras de hardware de la 8.2 | Pendiente (H8, H9) |
| P1: vigilancia de 1 s, ocultar valores | `vigilanciaMuestras` L3666–3699; `VIGILANCIA_UI_MS` L2923 | Autor (aprobó P1) | 2f, E10, H10 | Solo simulación (`simulacion_falla_sensor_9.png`) | Pendiente de ejecución |
| P2: reinicialización a los 3 s | L3691–3698; `VIGILANCIA_REINIT_MS` L2924; reintento L4074–4087 | Autor (aprobó P2) | 2f, E11, E12, H10 | Solo simulación | Pendiente de ejecución |
| P3: registro de arranque persistente | `arranque*` L3633–3658; `arrIni*` L2954–2963 | Autor (aprobó P3 y su corrección) | 2f, E1, E13, H11 | Solo simulación | Pendiente de ejecución |
| P4: sumas de 64 bits y tramos `z` | L2914, L3308; `tramoReiniciar` L3714 | Autor (aprobó P4) | 2f, E16, H12 | Solo simulación | Pendiente de ejecución |
| Pruebas guiadas `t`, `v`, `u`, `w` | L963–1106, L1644–1768, L2107–2218, L2565–2686 | Autor (criterios por etapa) | 2g, E14, H5, H6 | 4.1 (`t`) y 6.2 (`w`) aprobadas en hardware | Hardware previo; Etapa 9 pendiente |
| 22 comandos del Monitor Serie | `handleSerialCommands` L3941–4024 | Autor y asistente por etapa | 2f, E15 | Sin evidencia archivada | Pendiente de ejecución |
| Avisos de prototipo no médico | Cabecera L29–33; pantalla de inicio L3160–3185; pie “ESTIM. EXPERIMENTAL” | Autor | 2e, H7 | Simulación de pantallas | Pendiente (H7) |

## Inconsistencias encontradas (lista E)

Se encontraron 12 inconsistencias. **Ninguna se corrigió**: cada una espera tu autorización. Ninguna afecta a los algoritmos validados. En los documentos se describe el comportamiento real del código y se remite a esta lista.

| ID | Inconsistencia | Dónde está | Corrección propuesta |
| --- | --- | --- | --- |
| I1 | Se dice que `Etapa9.ino` tiene 4098 líneas; tiene 4112 (el MD5 sí coincide) | Plan de pruebas, sección “Alcance e inventario”; `tests/REPORTE_Categoria1.md`, línea 3 | **Corregido en el repositorio** el 2026-10-09, con autorización del autor: `docs/pruebas/plan-pruebas-etapa10.md` y `tests/unitarias/REPORTE_Categoria1.md` dicen 4112 |
| I2 | Los mensajes dicen que la corriente de LED llega a 16 mA y baja a 1 mA, pero desde 0x1F con paso 0x05 solo se alcanzan 15.2 mA (0x4C) y 1.2 mA (0x06) | Firmware: comentarios L174–175, mensajes L3987 y L3991; plan, H4 y 2f | Firmware sin cambios. Documentar el rango real de 1.2 a 15.2 mA y ajustar el criterio de H4 y 2f a los mensajes que imprime el firmware |
| I3 | El comentario dice “Sub-ventanas de \~1 s”; el código usa 200 muestras, ≈ 2 s | Firmware L358 contra L333 y L898 | Documentar 2 s; no tocar el firmware |
| I4 | El comentario de `UI_PPG_GAP = 4` dice “columnas borradas”; el código borra **una** columna situada 4 posiciones por delante del cursor | Firmware L3267 contra L3413–3414 | Documentar el comportamiento real |
| I5 | El texto impreso dice “Calidad (informativa)” y “sin conversion a SpO2”, pero desde la Etapa 6.2 esos umbrales sí deciden si la SpO₂ es válida | Firmware L2059 y L2064 (salida de `i`) | Documentar que son criterios activos (`oxOnBeat` L2419) |
| I6 | La cabecera asocia la falta de datos solo al pie “SENSOR NO RESPONDE”; durante la reinicialización P2 el pie muestra “SENSOR NO DETECTADO” | Firmware L8–10 contra L3566 | Documentar la secuencia: NO RESPONDE (1 s) y luego NO DETECTADO (desde 3 s, mientras se reintenta) |
| I7 | Se habla de “11 criterios” de la Etapa 4.1, pero la lista reportada tiene 12 renglones | Adjunto `6b7206cd` | Documentar los 11 criterios de la prueba `t` (L1020–1030) más dos observaciones (reinicios esperados y por artefacto) |
| I8 | El plan afirma que la equivalencia con la 6.2 “ya \[está\] demostrad\[a\] en las Etapas 7.3, 8.2 y 9”; fue una comparación en PC y no hay reporte archivado | Plan, flujo E17 | Etiquetarla como simulación; E17 queda pendiente de ejecución |
| I9 | Los archivos del enfoque inicial mencionan un condensador de 100 nF y un sistema con WiFi, servidor y base de datos que no forma parte del prototipo | `monitor_ppg_spo2.ino`, `monitor_ppg_corregido.ino`, `servidor.py`, `schema.sql` | No documentar el 100 nF ni el servidor salvo que confirmes que existen; hoy solo consta el de 470 µF / 25 V |
| I10 | La cabecera del firmware dice “NodeMCU ESP-32S (ESP32-WROOM-32)”; tus mensajes solo dicen “ESP32-WROOM-32” | Firmware L23 | Confirmar el nombre de la tarjeta de desarrollo |
| I11 | `manualResets` y `hrTimeouts` se cuentan pero no se imprimen en ninguna salida | Firmware L298, L3981, L406, L1520 | Documentarlo como limitación del diagnóstico |
| I12 | El manual pide explicar “SEÑAL MALA”, pero en la pantalla no existe ese texto: es la etiqueta “SEÑAL” sobre el campo de calidad “MALA” | Firmware L3072–3075 y L2986–2994 | Explicarlo así en el manual |

Además, el análisis de la Categoría 1 dejó dos hallazgos sin cambios en el firmware. Primero, con media ≤ 0, `spCvOf` devuelve 0 y `subCvPP` devuelve 99. Segundo, el estado FUERA DE RANGO por mediana de R no se alcanza por el flujo normal.

## Información faltante (lista F)

Para cerrar la documentación faltan 14 datos. En los documentos van marcados como “\[pendiente\]” o “Pendiente de ejecución”.

| ID | Falta | Dónde se necesita | Quién la aporta |
| --- | --- | --- | --- |
| F1 | Nombre del autor, institución, programa, profesor y fecha de entrega | Portada de los tres documentos | Autor |
| F2 | Resultados de H0–H13 sobre la Etapa 9: salida de `y`, informes de `t`, `v`, `u`, `w`, fotos y video | Capítulos 33–35 y expediente | Autor, en hardware |
| F3 | Fuente de alimentación real (USB de computadora u otra) y dónde está conectado el condensador de 470 µF / 25 V | Capítulo 16 y manual | Autor |
| F4 | Confirmar si existe un condensador de 100 nF (I9) y el nombre exacto de la tarjeta (I10) | Capítulos 13–15 | Autor |
| F5 | Fotografías del prototipo, del montaje y de cada pantalla | Anexos y expediente | Autor |
| F6 | Log original de la Etapa 7.3: número de errores I2C, duración, tasa medida y separación máxima | Capítulo 34 | Autor, si lo conserva |
| F7 | Cifras físicas de las etapas 5, 6.1 y 8.2 (BPM medidos, R de la 6.1, tiempo de dibujo de la gráfica) | Capítulo 34 | Autor, si las conserva |
| F8 | Fechas de cada etapa | Cronología y expediente | Autor (hoy solo constan las de los adjuntos) |
| F9 | Versiones exactas del núcleo Arduino-ESP32 y de las librerías (SparkFun MAX3010x, Adafruit ST7735, GFX, BusIO) | Capítulo 17 y H0 | Autor (Arduino IDE → Gestor de librerías) |
| F10 | Memoria de programa y RAM al compilar (H0) | Capítulo 33 | Autor |
| F11 | Disponibilidad de un oxímetro comercial de referencia | Capítulos 22, 35 y 37 | Autor; si no hay, se mantiene “NO EVALUADA” |
| F12 | Confirmar si el profesor fijó el 85 % de cobertura como requisito | Criterios de la Etapa 10 | Autor |
| F13 | Normas o lineamientos que el curso exija citar | Capítulo 41 | Autor; no se cita ninguna norma como “utilizada” |
| F14 | Resultados de las categorías 2a–2g y de los flujos E1–E17 en PC | Capítulos 31–32 | Pendiente de ejecución (requiere tu autorización para implementarlas) |
