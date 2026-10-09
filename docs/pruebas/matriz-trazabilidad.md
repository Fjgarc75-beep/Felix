# Matriz de trazabilidad

> Versión documentada: `firmware/etapa9-validada/Etapa9/Etapa9.ino`, MD5 `4d70eb1aef89be2d7ab61ec6d39b9f12`. Exportado al repositorio el 2026-10-09.

> **Origen y fuentes.** “Autor” es el autor del proyecto, que ejecutó y reportó las pruebas físicas. “Asistente” es el asistente de IA (Claude) que redactó el código y los documentos bajo la revisión y aprobación del autor. “Adjunto `xxxxxxxx`” identifica un reporte de resultados que el autor entregó durante el desarrollo; esos reportes son resúmenes y no incluyen el log completo.

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


Fuente: [inventario documental](../inventario-documental.md). Los códigos 2a–2g, E1–E17 y H0–H13 remiten al [plan de pruebas](plan-pruebas-etapa10.md).
