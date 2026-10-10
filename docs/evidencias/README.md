# Expediente de evidencias – Monitor PPG (Etapa 9)

> Índice de evidencias de la línea base `Etapa9.ino` (MD5 `4d70eb1aef89be2d7ab61ec6d39b9f12`). Actualizado el 2026-10-09. Las fotografías y los logs de hardware **no existen todavía**: sus filas están marcadas “Pendiente de ejecución”.

De 42 evidencias previstas, 11 están disponibles (7 de ellas son de PC o simulación), 5 son parciales, 25 están pendientes de ejecución y 1 no puede evaluarse sin un oxímetro de referencia. Firmware de referencia: `Etapa9.ino`, MD5 `4d70eb1aef89be2d7ab61ec6d39b9f12`. Ninguna evidencia de hardware de la Etapa 9 existe todavía.

## Índice de evidencias

Cada fila es una evidencia. La columna Estado se puede cambiar desde la tabla al agregar el archivo correspondiente. “Disponible (PC o simulación)” no cuenta como evidencia de hardware.

| ID | Evidencia | Categoría | Fecha | Prueba relacionada | Resultado | Archivo, fotografía o log | Estado |
| --- | --- | --- | --- | --- | --- | --- | --- |
| EV-01 | Firmware Etapa9.ino, 4112 líneas, 154 funciones | Versión y MD5 | 2026-10-07 | Todas | MD5 4d70eb1aef89be2d7ab61ec6d39b9f12 verificado | firmware/etapa9-validada/Etapa9/Etapa9.ino | Disponible |
| EV-02 | Diffs de la Etapa 9 contra la 8.2 y corrección de P3 | Versión y MD5 | — | P3 | Revisados; sin cambios en los algoritmos | firmware/etapa9-validada/Etapa9\_vs\_Etapa8\_2.diff; firmware/etapa9-validada/Etapa9\_correccionP3.diff | Disponible |
| EV-03 | Firmwares y MD5 de las etapas 1 a 8.2 | Versión y MD5 | — | Historial | 13 MD5 registrados en el inventario | firmware/historial/ | Disponible |
| EV-04 | Versiones del núcleo Arduino-ESP32 y de las librerías | Versión y MD5 | — | H0 | — | \[pendiente: F9\] | Pendiente de ejecución |
| EV-05 | Fotografía del prototipo completo | Fotografías | — | H7 | — | \[pendiente: F5\] | Pendiente de ejecución |
| EV-06 | Fotografía del montaje y del cableado I²C y SPI | Fotografías | — | H1, H9 | — | \[pendiente: F5\] | Pendiente de ejecución |
| EV-07 | Fotografía del condensador de 470 µF y su ubicación | Fotografías | — | Cap. 16 | — | \[pendiente: F3\] | Pendiente de ejecución |
| EV-08 | Fotos o video de la TFT en cada estado de calidad y en el aviso inicial | Capturas | — | H7 | — | \[pendiente\] | Pendiente de ejecución |
| EV-09 | Video del barrido de la gráfica PPG con cronómetro | Capturas | — | H8 | — | \[pendiente\] | Pendiente de ejecución |
| EV-10 | Pantallas simuladas de la Etapa 7.3 | Capturas | — | Etapa 7.3 | Ilustrativo | [simulaciones/simulacion\_pantallas\_7\_3.png](simulaciones/simulacion_pantallas_7_3.png) | Disponible (PC o simulación) |
| EV-11 | Pantallas simuladas de la Etapa 8.2 | Capturas | — | Etapa 8.2 | Ilustrativo | [simulaciones/simulacion\_pantallas\_8\_2.png](simulaciones/simulacion_pantallas_8_2.png) | Disponible (PC o simulación) |
| EV-12 | Secuencia simulada de falla y recuperación del sensor | Capturas | — | P1, P2 | NO RESPONDE a 41.5 s, NO DETECTADO a 43.2 s, recuperado a 46.0 s | [simulaciones/simulacion\_falla\_sensor\_9.png](simulaciones/simulacion_falla_sensor_9.png) | Disponible (PC o simulación) |
| EV-13 | Prueba guiada t de la Etapa 4.1 | Resultados de pruebas | 2026-09-29 (reporte) | t | 11 criterios OK; 99.1 muestras/s; 0 pérdidas; 55.4×; corr. 0.98 | Adjunto 6b7206cd (resumen, sin log) | Parcial |
| EV-14 | Prueba guiada w de la Etapa 6.2 | Resultados de pruebas | 2026-10-01 (reporte) | w | 35 latidos; R ≈ 0.375; CV ≈ 4 %; SpO₂ ≈ 99.9 %; concordancia NO EVALUADA | Adjunto 37499aa3 (resumen, sin log) | Parcial |
| EV-15 | Verificación de la TFT, Etapa 7.1 | Resultados de pruebas | 2026-10-01 (reporte) | Etapa 7.1 | 8 pruebas; 14998 cuadros en 5.0 min; fillScreen ≈ 24.46 ms | Adjunto a39ddc66 (resumen) | Parcial |
| EV-16 | Diagnóstico de la interfaz, Etapa 7.2 | Resultados de pruebas | 2026-10-01 (reporte) | Etapa 7.2 | Peor caso ≈ 18.28 ms; heap ≈ 327960 bytes | Adjunto 37499aa3 (resumen) | Parcial |
| EV-17 | Integración sensor y TFT, Etapa 7.3 | Resultados de pruebas | 2026-10-01 (reporte) | Etapa 7.3 | ≈ 18.30 ms; 14 pérdidas y 1 desbordamiento acumulados | Adjunto 7e7693dc y resumen del historial | Parcial |
| EV-18 | Log original de la Etapa 7.3 con los errores I²C | Logs | — | H9, H11 | — | \[pendiente: F6\] | Pendiente de ejecución |
| EV-19 | Salida del comando i con los registros del sensor | Logs | — | H1 | — | \[pendiente\] | Pendiente de ejecución |
| EV-20 | Salidas de y al inicio y cada 10 min durante 30 min | Logs | — | H2 | — | \[pendiente\] | Pendiente de ejecución |
| EV-21 | Cinco encendidos: líneas \[ARRANQUE\] y salida de y | Arranque (P3) | — | H11, E1 | — | \[pendiente\] | Pendiente de ejecución |
| EV-22 | Encendido sin sensor y conexión posterior | Arranque (P3) | — | E13 | — | \[pendiente\] | Pendiente de ejecución |
| EV-23 | SDA desconectado 2 s | Pérdida de sensor (P1) | — | H10, E10 | — | \[pendiente\] | Pendiente de ejecución |
| EV-24 | SDA desconectado 10 s | Pérdida de sensor (P1 y P2) | — | H10, E11 | — | \[pendiente\] | Pendiente de ejecución |
| EV-25 | Pérdida de configuración del sensor (POR\_AT) | Pérdida de sensor (P2) | — | E12, solo PC | — | \[pendiente\] | Pendiente de ejecución |
| EV-26 | Tramos con z y sumas de 64 bits | Tiempos (P4) | — | H2, H9 | — | \[pendiente\] | Pendiente de ejecución |
| EV-27 | Gráfica activa frente a desactivada, 10 min cada una | Tiempos (P4) | — | H9 | — | \[pendiente\] | Pendiente de ejecución |
| EV-28 | 60 min con dedo intermitente: motivo de reinicio y memoria | Larga duración | — | H12 | — | \[pendiente\] | Pendiente de ejecución |
| EV-29 | 60 min de funcionamiento continuo | Larga duración | — | H13 | — | \[pendiente\] | Pendiente de ejecución |
| EV-30 | 24 h virtuales en PC | Larga duración | — | E16 | — | \[pendiente\] | Pendiente de ejecución |
| EV-31 | Compilación con el núcleo real y memoria usada | Resultados de pruebas | — | H0 | — | \[pendiente: F10\] | Pendiente de ejecución |
| EV-32 | Señal óptica real en modos k y d | Resultados de pruebas | — | H3 | — | \[pendiente\] | Pendiente de ejecución |
| EV-33 | Corriente de LED con + y - | Resultados de pruebas | — | H4 | — | \[pendiente\] | Pendiente de ejecución |
| EV-34 | Prueba v y conteo manual del pulso de 60 s | Resultados de pruebas | — | H5 | — | \[pendiente\] | Pendiente de ejecución |
| EV-35 | Prueba w repetida tres veces | Resultados de pruebas | — | H6 | — | \[pendiente\] | Pendiente de ejecución |
| EV-36 | Plan de pruebas de la Etapa 10, revisión 19 | Etapa 10 | — | Todas | Categorías 1, 2a–2g, H0–H13 y E1–E17 | [Plan de pruebas](../pruebas/plan-pruebas-etapa10.md) | Disponible |
| EV-37 | Resultados de la Categoría 1 (CSV, Markdown, JUnit) | Etapa 10 | 2026-10-09 | Cat. 1 | 116 de 116 casos aprobados | tests/unitarias/reports/cat1\_resultados.*; cat1\_variante\_dec0\_* | Disponible (PC o simulación) |
| EV-38 | Cobertura de la Categoría 1 | Etapa 10 | 2026-10-09 | Cat. 1 | Líneas 257/257; ramas 200/200 (objetivo ≥ 85 %) | tests/unitarias/reports/cat1\_cobertura.md | Disponible (PC o simulación) |
| EV-39 | Prueba de mutación de la Categoría 1 | Etapa 10 | 2026-10-09 | Cat. 1 | 16 de 16 mutantes detectados | tests/unitarias/reports/cat1\_mutacion.md | Disponible (PC o simulación) |
| EV-40 | MD5 antes y después de la corrida | Etapa 10 | 2026-10-09 | Cat. 1 | Sin cambios | tests/unitarias/reports/cat1\_linea\_base.txt | Disponible (PC o simulación) |
| EV-41 | Categorías 2a–2g y flujos E1–E17 | Etapa 10 | — | 2a–2g, E1–E17 | — | \[pendiente\] | Pendiente de ejecución |
| EV-42 | Concordancia con un oxímetro de referencia | Limitaciones | — | H6 | NO EVALUADA: no hay equipo de referencia | \[pendiente: F11\] | No evaluada |

## Cómo registrar una evidencia nueva

1. **Antes de cada prueba en hardware**, verifique que el firmware cargado sea `Etapa9.ino` con MD5 `4d70eb1aef89be2d7ab61ec6d39b9f12` y anótelo en la fila.
2. **Nombre del archivo:** `EV-xx_AAAA-MM-DD_prueba_descripcion.ext`, por ejemplo `EV-24_2026-10-12_H10_SDA_10s.txt` o `EV-08_2026-10-12_H7_BUENA.jpg`.
3. **Carpetas sugeridas:**

```
expediente/
  01_version/      firmware, MD5, versiones de núcleo y librerías
  02_fotos/        prototipo, montaje, condensador
  03_pantalla/     fotos y video de la TFT
  04_logs/         salidas del Monitor Serie (i, y, [ARRANQUE], pruebas t v u w)
  05_hardware/     H0–H13, una subcarpeta por prueba
  06_pc/           reportes de tests/ (Categoría 1 y siguientes)
  07_limitaciones/ notas sobre lo no evaluado
```

4. **Logs:** copie la salida completa del Monitor Serie a un archivo `.txt` sin editarla. Incluya la salida de `y` al inicio y al final de la prueba.
5. **Fotos y video:** que se vea la pantalla completa y, si es posible, un reloj o cronómetro.
6. **Resultado:** escriba el valor medido con su unidad, la condición (dedo quieto, SDA desconectado, etc.), la duración y si cumple el criterio del plan. Solo cambie el Estado a “Disponible” cuando el archivo esté guardado.
7. **Simulación y hardware no se mezclan:** una evidencia de PC lleva siempre el estado “Disponible (PC o simulación)”.

Las inconsistencias (I1–I12) y la información faltante (F1–F14) están en el [Inventario documental](../inventario-documental.md).
