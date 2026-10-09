# Limitaciones y seguridad

> Versión documentada: `firmware/etapa9-validada/Etapa9/Etapa9.ino`, MD5 `4d70eb1aef89be2d7ab61ec6d39b9f12`. Exportado al repositorio el 2026-10-09.

> **Prototipo académico y experimental.** No es un dispositivo médico certificado, no sustituye a un monitor clínico, la SpO₂ no tiene validación clínica y no debe usarse para diagnóstico ni para decisiones de salud. El proyecto se centra en la adquisición, el procesamiento, la visualización y la validación experimental del sistema.

## 1. Resumen

Las limitaciones principales son tres. Se detallan en las secciones 2 y 3.

1. **Es un prototipo académico.** No es un dispositivo médico certificado, no sustituye a un monitor clínico y no debe usarse para diagnóstico.
2. **La SpO₂ no tiene validación clínica.** Los coeficientes de la curva son genéricos, sin calibrar para este módulo, y no se compararon contra un oxímetro de referencia. La exactitud clínica no se demostró.
3. **La Etapa 9 no está validada en hardware.** Las pruebas físicas H0–H13 y las de integración en PC (2a–2g, E1–E17) están pendientes de ejecución.

El proyecto se centra en la adquisición, el procesamiento, la visualización y la validación experimental del sistema, no en la exactitud clínica.

## 2. Limitaciones técnicas

| Limitación | Efecto | Dónde |
| --- | --- | --- |
| Validación de la Etapa 9 en hardware pendiente de ejecución | P1–P4 solo se probaron en simulación | H0–H13 |
| Pruebas de integración en PC pendientes de ejecución | 109 funciones y 17 flujos sin prueba automatizada | Categorías 2a–2g, E1–E17 |
| Filtros y envolvente calculados con fs nominal de 100 Hz, no con la medida (≈ 99.1) | Desplazamiento de ≈ 0.9 % en las frecuencias de corte y en las constantes de tiempo (cálculo propio, no medido) | L548–571, L1181 |
| `measuredFs` es un promedio desde el arranque | Tras un periodo sin muestras puede bajar o quedar congelada (deducido del código, no probado) | L1145–1157 |
| `lostSamples` cuenta solo lo que informa OVF (máximo 31 por lectura) | Es una cota mínima de las pérdidas reales | L2761–2767 |
| Corriente de LED fija con ajuste manual | No compensa distintos tonos de piel, grosores o temperaturas | L173–176 |
| Rango real de corriente 1.2–15.2 mA, aunque los mensajes dicen 1 y 16 mA | Mensaje inexacto (I2) | L3987, L3991 |
| Sin uso del pin INT | El sondeo cada 10 ms depende de que el lazo no se bloquee más de ≈ 320 ms | L38, L189 |
| Reintentos de inicialización sin límite | No hay estado final de falla ni aviso escalonado | L4074–4087 |
| Dos contadores sin salida (`manualResets`, `hrTimeouts`) | Diagnóstico incompleto (I11) | L298, L406 |
| Pruebas unitarias compiladas con g++ en PC, no con el compilador Xtensa | Pueden diferir los últimos bits de los cálculos en `float` | Arnés de pruebas |
| Sin registro ni transmisión de datos | No hay historial ni integración con un sistema de información | L116 |

## 3. Limitaciones clínicas

- Es un **prototipo académico y experimental**. **No es un dispositivo médico certificado** y no sustituye a un monitor clínico.
- **La SpO₂ no tiene validación clínica.** Usa coeficientes genéricos no calibrados y no se comparó contra un oxímetro de referencia ni contra gasometría arterial.
- **La exactitud clínica no se demostró**, ni para la SpO₂ ni para el BPM. La única comparación prevista para el BPM (H5) es informal: un conteo manual del pulso.
- El único resultado de SpO₂ disponible (≈ 99.9 % en reposo) sugiere que la curva sobreestima la saturación para este módulo.
- El sistema solo se probó con el dedo en reposo. No se evaluó con baja perfusión, movimiento, arritmias, distintas pigmentaciones de piel, uñas pintadas, hipotermia, dishemoglobinas ni saturaciones bajas.
- No tiene alarmas: no avisa de valores fuera de rango.
- **No debe usarse para diagnóstico** ni para tomar decisiones de salud.

## 4. Seguridad y consideraciones de uso

**Seguridad eléctrica.** El prototipo trabaja a 3.3 V y se alimenta a través de la tarjeta del ESP32 (fuente por confirmar: F3). Sus partes conductoras están expuestas y no tiene aislamiento de grado médico, por lo que no se ha evaluado frente a ninguna norma de seguridad de equipo electromédico y no debe considerarse conforme a ellas. Recomendaciones:

- Usarlo solo con fuentes USB en buen estado y sin conectarlo a equipos conectados a la red eléctrica que estén en contacto con un paciente.
- No usarlo sobre piel lesionada, en presencia de líquidos ni en personas con dispositivos implantados, salvo indicación de un profesional.

**Seguridad óptica.** Los LED son de baja potencia: 6.2 mA por defecto y como máximo ≈ 15.2 mA con el ajuste manual. Aun así, no se debe mirar directamente al sensor encendido.

**Higiene.** El sensor toca la piel. Hay que limpiarlo entre usuarios con un paño apenas humedecido en alcohol isopropílico, sin mojar la electrónica.

**Uso correcto de la información.** El firmware aplica varias medidas para que un dato dudoso no se muestre:

- avisa en la pantalla de inicio que no es un dispositivo médico;
- muestra el pie “ESTIM. EXPERIMENTAL”;
- oculta los valores (“--”) cuando el BPM o la SpO₂ no son válidos o cuando los datos se congelan.

Estas medidas reducen la posibilidad de mostrar un valor no confiable, pero **no** convierten el equipo en una herramienta clínica. Ante cualquier situación médica debe usarse equipo clínico apropiado y consultarse a personal sanitario.

Las inconsistencias I1–I12 y la información faltante F1–F14 están en el [inventario documental](inventario-documental.md).
