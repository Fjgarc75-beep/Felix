# Pruebas sobre hardware (H0–H13) – Pendiente de ejecución

Son las pruebas que deciden la validación de la Etapa 9 sobre el ESP32, el MAX30102 y la TFT reales. **Ninguna se ha ejecutado todavía.** Por eso la Etapa 9 no está validada en hardware.

Antes de cada prueba, verifique que el firmware cargado sea `firmware/etapa9-validada/Etapa9/Etapa9.ino` con MD5 `4d70eb1aef89be2d7ab61ec6d39b9f12`.

| ID | Prueba | Procedimiento | Criterio | Estado |
| --- | --- | --- | --- | --- |
| H0 | Compilación con el núcleo real | Arduino IDE, Arduino-ESP32 3.3.x | Sin errores; memoria anotada | Pendiente de ejecución |
| H1 | Configuración del sensor | Comando `i` | PART_ID 0x15; registros OK | Pendiente de ejecución |
| H2 | Adquisición sin pérdidas | `z`, 30 min con dedo, `y` cada 10 min | 0 errores I²C, 0 pérdidas de operación, fs 98–102 | Pendiente de ejecución |
| H3 | Señal óptica | Modo `k`, modo `d` con y sin dedo | Pulso visible; DC IR ≥ 50000 con dedo | Pendiente de ejecución |
| H4 | Corriente de LED | `+` y `-` con dedo | El DC sigue a la corriente | Pendiente de ejecución |
| H5 | BPM | Prueba `v` y conteo manual de 60 s | `v` aprobada; diferencia ≤ 5 BPM (referencia informal) | Pendiente de ejecución |
| H6 | SpO₂ experimental | Prueba `w`, tres veces | Criterios automáticos OK; concordancia NO EVALUADA | Pendiente de ejecución |
| H7 | Imagen de la pantalla | Fotos y video de cada estado | Distribución B, Ñ visible, sin parpadeo | Pendiente de ejecución |
| H8 | Barrido de la gráfica | Cronómetro | 5.1 ± 0.3 s por pantalla | Pendiente de ejecución |
| H9 | Interacción SPI–I²C | 10 min con gráfica y 10 min sin ella | Sin diferencia en errores ni pérdidas | Pendiente de ejecución |
| H10 | P1 y P2 | Desconectar SDA 2 s y 10 s | Reposición sola; recuperación 1/1 sin reiniciar el ESP32 | Pendiente de ejecución |
| H11 | Arranque (P3) | 5 encendidos | Origen de las 14 pérdidas identificado | Pendiente de ejecución |
| H12 | Alimentación y memoria | 60 min con dedo intermitente | Motivo de reinicio = encendido; memoria estable ±1 KB | Pendiente de ejecución |
| H13 | Funcionamiento prolongado | 60 min continuos | Sin reinicios ni valores congelados | Pendiente de ejecución |

Registre cada resultado como evidencia (log del Monitor Serie sin editar, fotos o video) según el [expediente de evidencias](../../docs/evidencias/README.md), y actualice [resultados-pruebas.md](../../docs/pruebas/resultados-pruebas.md).

Inconsistencia conocida que afecta a H4: los mensajes del firmware dicen 16 mA y 1 mA, pero el rango real de la corriente es 1.2–15.2 mA (I2, [inventario](../../docs/inventario-documental.md)).
