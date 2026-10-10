# Pruebas de integración en PC – Pendiente de ejecución

Estas pruebas están **definidas en el plan, pero todavía no se han implementado ni ejecutado**. Esta carpeta no contiene pruebas: no se crearon archivos vacíos para simular avance.

| Bloque | Qué verificará | Estado |
| --- | --- | --- |
| 2a (14 funciones) | I²C, FIFO, desbordamiento, fs medida, ventanas | Pendiente de ejecución |
| 2b (5) | DC, filtros, reinicios y estabilización contra Python | Pendiente de ejecución |
| 2c (10) | Detector de BPM de 40 a 180 BPM con muesca dicrótica y ruido | Pendiente de ejecución |
| 2d (15) | R con latidos sintéticos de R conocido; estados de SpO₂ | Pendiente de ejecución |
| 2e (25) | Pantalla píxel a píxel; actualización parcial; gráfica | Pendiente de ejecución |
| 2f (12) | P1–P4 con fallas inyectadas; 22 comandos | Pendiente de ejecución |
| 2g (26) | Pruebas guiadas `t`, `v`, `u`, `w` con escenarios de aprobación y de falla | Pendiente de ejecución |
| E1–E17 | `setup()` y `loop()` en flujos de extremo a extremo | Pendiente de ejecución |

Usarán el mismo arnés de PC que las pruebas unitarias ([`../unitarias/harness/`](../unitarias/harness/)): núcleo con reloj virtual, simulador del MAX30102 (FIFO, desbordamiento, dedo programable, corte de bus y pérdida de configuración) y TFT simulada.

Detalle de cada caso: [plan de pruebas](../../docs/pruebas/plan-pruebas-etapa10.md), categorías 2a–2g y 4.
