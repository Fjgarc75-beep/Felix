# Manual de usuario – Monitor experimental de FC y SpO₂

> Manual del prototipo con el firmware de la Etapa 9 (MD5 `4d70eb1aef89be2d7ab61ec6d39b9f12`). Versión del 2026-10-09.

> **Equipo experimental. No es un dispositivo médico.** Los valores que muestra son orientativos y no deben usarse para tomar decisiones médicas. Ante cualquier situación médica, use equipo clínico apropiado y consulte a personal sanitario.

## 1. Introducción

Este manual explica cómo encender el monitor, cómo colocar el dedo y cómo leer lo que aparece en la pantalla. Está escrito para una persona que no conoce el programa del equipo, así que no necesita saber de electrónica ni de programación.

## 2. Propósito del equipo

El monitor es un **prototipo académico**. Se creó para aprender cómo un equipo obtiene el pulso y la saturación de oxígeno a partir de una luz que se refleja en el dedo. Muestra:

- la **frecuencia cardíaca** (FC), en latidos por minuto (BPM);
- una **saturación de oxígeno estimada** (SpO₂), en porcentaje;
- la **onda del pulso** (gráfica PPG) y una indicación de la calidad de la señal.

> **Importante.** El equipo es experimental. No está certificado como dispositivo médico, no sustituye a un monitor clínico y no debe usarse para diagnosticar ni para decidir un tratamiento. La SpO₂ que muestra es una estimación que no se ha comparado con un oxímetro clínico.

## 3. Componentes

| Pieza | Qué es | Para qué sirve |
| --- | --- | --- |
| Tarjeta ESP32 | La placa con el microcontrolador y el conector USB | Es el “cerebro”: hace todos los cálculos |
| Sensor MAX30102 | Un módulo pequeño con una ventanita y luces roja e infrarroja | Aquí se apoya el dedo |
| Pantalla TFT de 1.8" | Pantalla a color de 128 × 160 puntos | Muestra los valores y la onda |
| Condensador de 470 µF | Un cilindro pequeño en el circuito | Ayuda a estabilizar la alimentación |
| Cable USB | El cable de la tarjeta | Da energía al equipo |

## 4. Descripción del equipo

El sensor tiene dos luces: una roja y otra infrarroja, que no se ve a simple vista. Cuando se apoya el dedo, parte de esa luz se refleja y regresa al sensor. Con cada latido cambia un poco la cantidad de sangre en el dedo, y con ella la luz que regresa. El equipo mide ese cambio muchas veces por segundo y con él calcula el pulso y la saturación estimada.

La pantalla está dividida en zonas, de arriba abajo:

| Zona | Qué muestra |
| --- | --- |
| Encabezado | “MONITOR PPG” |
| FC / BPM | El pulso, en números verdes grandes |
| SpO2 / % | La saturación estimada, en números azul claro, con un decimal |
| SEÑAL | La calidad de la señal, en una palabra de color |
| Gráfica | La onda del pulso, que avanza de izquierda a derecha |
| Pie | Un aviso en amarillo; normalmente “ESTIM. EXPERIMENTAL” |

## 5. Conexiones

El equipo se entrega armado y no hace falta conectar nada entre sus piezas. **No desconecte ni cambie los cables** del sensor o de la pantalla: si un cable se mueve, el sensor puede dejar de responder.

Solo como referencia, por si un técnico necesita revisar el armado:

| Tarjeta ESP32 | Va a | Pin del módulo |
| --- | --- | --- |
| 3.3 V | Sensor | VIN |
| GND | Sensor | GND |
| GPIO21 | Sensor | SDA |
| GPIO22 | Sensor | SCL |
| 3.3 V | Pantalla | VCC y LED |
| GND | Pantalla | GND |
| GPIO18 | Pantalla | SCK |
| GPIO23 | Pantalla | SDA |
| GPIO16 | Pantalla | A0 |
| GPIO17 | Pantalla | RESET |
| GPIO5 | Pantalla | CS |

Los pines INT y RD del sensor van sin conectar.

## 6. Alimentación

El equipo se alimenta por el **cable USB** de la tarjeta ESP32. No usa baterías ni tiene interruptor.

- Use un puerto USB de computadora o un cargador USB en buen estado. \[Pendiente de confirmar por el autor la fuente recomendada.\]
- No use cables dañados ni cargadores sin marca.
- No conecte el equipo mientras esté mojado o sobre una superficie metálica.

## 7. Encendido

1. Coloque el equipo sobre una mesa firme, con la pantalla a la vista.
2. Conecte el cable USB. El equipo se enciende solo.
3. Durante unos **3 segundos** aparece una pantalla de aviso con estos textos: “PROTOTIPO ACADEMICO”, “NO ES DISPOSITIVO MEDICO” dentro de un recuadro amarillo, “ESTIMACION EXPERIMENTAL” e “Iniciando...”.
4. Después aparece la pantalla de medición:
   - en el lugar del pulso, “--”;
   - en el de la saturación, “--.-”;
   - en la señal, “SIN DEDO”;
   - la gráfica, como una línea plana gris;
   - el pie, “ESTIM. EXPERIMENTAL”.

Si el pie dice **“SENSOR NO DETECTADO”**, el equipo no encontró el sensor. Lo vuelve a buscar solo cada 5 segundos. Si el mensaje no desaparece, vea la sección 24.

## 8. Colocación correcta del dedo

La mayor parte de los problemas de medición se deben a cómo está apoyado el dedo. Siga estos pasos:

1. Siéntese y apoye el antebrazo y la mano sobre la mesa, a la altura del equipo.
2. Use la **yema** del dedo índice o del medio. Las manos deben estar limpias, secas y tibias.
3. Cubra por completo la ventanita del sensor con la yema.
4. Apoye el dedo con una **presión suave**, como si tocara un botón sin apretarlo. Si aprieta mucho, el pulso se debilita.
5. **No mueva** el dedo ni la mano y evite hablar mientras mide.
6. Evite la luz directa del sol o de una lámpara sobre el sensor.

Cosas que suelen dar lecturas malas: uñas pintadas o postizas, dedos fríos, temblor, presionar de más y mover el dedo.

## 9. Cómo iniciar una medición

No hay que pulsar ningún botón: **la medición empieza sola** al colocar el dedo.

1. Con el equipo encendido y la pantalla en “SIN DEDO”, coloque el dedo como se indica en la sección 8.
2. La palabra de calidad cambia a **BUSCANDO** y la gráfica empieza a dibujar la onda en gris. El equipo está ajustándose a su señal. Al principio espera unos 3 a 4 segundos y luego necesita varios latidos seguidos y parecidos entre sí.
3. Mantenga el dedo quieto. Cuando el pulso es estable, aparece el **número de BPM** en verde.
4. La saturación tarda un poco más, porque necesita al menos 5 latidos de buena calidad. Cuando aparece el número en azul claro, la calidad cambia a **BUENA** y la onda se vuelve verde.

Las pruebas de diseño del equipo esperan que el pulso aparezca en menos de 15 segundos y la saturación en menos de 20 segundos con el dedo quieto. Si tarda más, revise la colocación del dedo.

Si mueve el dedo durante la medición, el equipo vuelve a ajustarse y los números pueden cambiar a “--” unos segundos. Es normal.

## 10. Interpretación de BPM

El número verde grande es la **frecuencia cardíaca estimada**, en latidos por minuto. Es un promedio de los últimos latidos, así que cambia poco a poco y no de un latido a otro.

- **“--” en gris** significa que el equipo todavía no tiene un valor confiable: el dedo no está, se está ajustando o la señal es mala. No es un error.
- El equipo solo cuenta pulsos entre 40 y 180 BPM.
- Si el pulso es irregular, el número puede aparecer y desaparecer. El equipo prefiere no mostrar un valor antes que mostrar uno dudoso.

Como referencia general, en un adulto en reposo el pulso suele estar entre 60 y 100 latidos por minuto. **Este equipo no sirve para decidir si un valor es normal o no**: ante cualquier duda, use un equipo clínico.

## 11. Interpretación de SpO₂

El número azul claro, con un decimal y el signo %, es una **saturación de oxígeno estimada de forma experimental**.

- **“--.-” en gris** significa que no hay un valor confiable: faltan latidos de buena calidad, el pulso no es estable o la señal no cumple los criterios del equipo.
- El valor nunca pasa de 100.0 %.
- La fórmula que usa el equipo es genérica y no se ajustó a este sensor. En la única prueba registrada dio alrededor de 99.9 %, probablemente por encima del valor real.

> **No use la SpO₂ de este equipo para ninguna decisión.** No se comparó con un oxímetro clínico y su exactitud no está demostrada. Si alguien tiene dificultad para respirar, labios o dedos morados, o se siente mal, busque atención médica y use un oxímetro clínico.

## 12. Interpretación de la gráfica PPG

La gráfica de la parte baja muestra la **onda del pulso**: cada “montañita” es un latido. La onda avanza de izquierda a derecha y tarda unos 5 segundos en recorrer la pantalla. Después vuelve a empezar por la izquierda, borrando lo anterior.

| Cómo se ve | Qué significa |
| --- | --- |
| Línea plana gris | No hay dedo sobre el sensor |
| Onda gris | Hay señal, pero el equipo todavía la está ajustando o es de mala calidad |
| Onda verde | La calidad es BUENA o REGULAR |
| Línea cortada | Se perdieron datos un momento; la escala se reajusta sola |
| “SIN DATOS DEL SENSOR” en naranja | El sensor dejó de enviar datos (sección 16) |
| “GRAFICA DESACTIVADA” | La gráfica se apagó desde la computadora; el resto sigue funcionando |

El tamaño de la onda se ajusta solo, así que **una onda grande no significa un pulso más fuerte**. Lo importante es que las montañitas sean parecidas y regulares.

## 13. Estados de calidad

Debajo de la palabra “SEÑAL” aparece una de cinco palabras, cada una con su color:

| Palabra | Color | Qué significa | Qué hacer |
| --- | --- | --- | --- |
| **BUENA** | Verde | Pulso y saturación estimada válidos | Nada; mantenga el dedo quieto |
| **REGULAR** | Amarillo | El pulso es válido, pero la saturación no cumple los criterios de calidad | Mantenga el dedo quieto y con presión suave; espere |
| **BUSCANDO** | Gris claro | El equipo se está ajustando o el sensor no está listo | Espere unos segundos con el dedo quieto |
| **MALA** | Naranja | Hay dedo, pero la señal es muy débil o la luz satura el sensor | Recoloque el dedo (sección 15) |
| **SIN DEDO** | Gris claro | No hay dedo sobre el sensor | Coloque el dedo |

## 14. Qué significa “BUSCANDO”

“BUSCANDO” no es una falla: el equipo está trabajando para obtener un valor confiable. Aparece en estos casos:

- justo después de colocar el dedo, mientras se ajusta (los primeros segundos);
- cuando el pulso todavía no es estable;
- cuando el pulso ya es válido pero la saturación aún está juntando latidos;
- después de un movimiento brusco del dedo;
- cuando el sensor no responde o se está reiniciando (en ese caso el pie lo indica; ver sección 16).

Si “BUSCANDO” dura más de unos 30 segundos con el dedo quieto, retire el dedo, espere a que aparezca “SIN DEDO” y colóquelo de nuevo.

## 15. Qué significa “SEÑAL MALA”

En la pantalla se ve la etiqueta “SEÑAL” y debajo la palabra **MALA** en naranja. Hay dedo sobre el sensor, pero el equipo no puede usar la señal por una de dos razones:

- **Señal débil:** el dedo está apenas apoyado, está frío o se presiona demasiado.
- **Saturación del sensor:** entra demasiada luz, por ejemplo luz directa del sol o de una lámpara.

Qué hacer:

1. Retire el dedo y vuelva a colocarlo cubriendo toda la ventanita.
2. Ajuste la presión: ni muy suave ni muy fuerte.
3. Aléjese de la luz directa o cubra la mano.
4. Si tiene los dedos fríos, caliéntelos frotándolos un momento.

## 16. Qué hacer si aparece “SENSOR NO RESPONDE”

Este mensaje aparece en el pie cuando el sensor deja de enviar datos durante 1 segundo. Para no mostrar valores viejos, el equipo hace tres cosas:

- borra los números, que pasan a “--” y “--.-”;
- pone la calidad en BUSCANDO;
- escribe “SIN DATOS DEL SENSOR” en la gráfica.

Qué pasa después:

- Si los datos vuelven solos, por ejemplo después de un falso contacto breve, todo regresa a la normalidad sin hacer nada.
- Si a los 3 segundos siguen sin llegar datos, el equipo **reinicia el sensor por sí mismo**. Mientras lo hace, el pie cambia a “SENSOR NO DETECTADO”. Cuando el sensor responde, la medición empieza de nuevo.

Qué hacer:

1. No toque los cables mientras el equipo está conectado.
2. Espere unos 10 segundos para darle tiempo a recuperarse.
3. Si el mensaje sigue, desconecte el cable USB, espere 5 segundos y vuelva a conectarlo.
4. Si persiste, pida a un técnico que revise los cables del sensor (sección 5).

## 17. Qué hacer si no aparecen datos

| Lo que ve | Causa probable | Qué hacer |
| --- | --- | --- |
| Pantalla totalmente apagada | No llega energía | Revise el cable USB y el cargador; pruebe otro puerto |
| Pantalla blanca o con colores extraños | La pantalla no arrancó bien | Desconecte y vuelva a conectar el USB |
| “--” y “SIN DEDO” con el dedo puesto | El dedo no cubre la ventanita | Recoloque el dedo (sección 8) |
| “--” y “BUSCANDO” mucho tiempo | Movimiento, dedo frío o presión excesiva | Siga la sección 14 |
| BPM visible pero SpO₂ en “--.-” | La señal no cumple los criterios para estimar la saturación | Mantenga el dedo quieto; si sigue así, recolóquelo |
| Pie “SENSOR NO DETECTADO” | El sensor no responde | Siga la sección 16 |

## 18. Cómo retirar el dedo

1. Levante el dedo con suavidad, sin arrastrarlo sobre el sensor.
2. En pocos instantes la pantalla muestra “--”, “--.-” y “SIN DEDO”, y la gráfica queda como una línea plana gris.
3. Si quiere medir de nuevo, espere a ver “SIN DEDO” y vuelva a colocar el dedo.

Los valores no se guardan: al retirar el dedo desaparecen de la pantalla.

## 19. Apagado

El equipo no tiene interruptor. Para apagarlo:

1. Retire el dedo del sensor.
2. Desconecte el cable USB tomando el conector, sin tirar del cable.

No hace falta ningún paso previo: el equipo no guarda información.

## 20. Limpieza y cuidado

- Desconecte el equipo antes de limpiarlo.
- Limpie la ventanita del sensor con un paño suave apenas humedecido en alcohol isopropílico, entre una persona y otra. No moje la electrónica ni la pantalla.
- No use solventes, aerosoles ni agua directa.
- Guárdelo en un lugar seco, sin golpes y sin peso encima de los cables.
- No doble los cables ni tire de ellos.

## 21. Precauciones

> **El equipo es experimental.** No debe usarse para tomar decisiones médicas. Los valores son orientativos. Ante una situación médica, use equipo clínico apropiado y consulte a personal sanitario.

- No lo use en pacientes, en hospitalización ni en situaciones de urgencia.
- No lo use sobre piel lastimada ni en presencia de líquidos.
- No mire directamente las luces del sensor encendidas.
- No lo conecte a equipos que estén en contacto con un paciente.
- Manténgalo fuera del alcance de los niños: tiene piezas pequeñas y cables expuestos.
- Si nota calor, olor a quemado o humo, desconecte el cable USB de inmediato.

## 22. Limitaciones

- Es un **prototipo académico**; no es un dispositivo médico certificado.
- **No sustituye** a un monitor ni a un oxímetro clínico.
- **La SpO₂ no tiene validación clínica**: la fórmula es genérica y no se comparó con un oxímetro de referencia.
- La exactitud del pulso y de la saturación **no está demostrada**.
- Solo se probó con el dedo en reposo. El movimiento, el frío, la mala circulación, el esmalte de uñas y la luz intensa afectan la medición.
- No tiene alarmas: no avisa si un valor es alto o bajo.
- No guarda ni envía los datos.
- **No debe usarse para diagnóstico.**

## 23. Preguntas frecuentes

**¿Puedo usar este equipo para saber si estoy bien de salud?** No. Es un prototipo experimental y sus valores son orientativos. Para cualquier decisión de salud, use equipo clínico y consulte a personal sanitario.

**¿Por qué tarda en aparecer el número?** El equipo espera hasta tener varios latidos seguidos y parecidos. Así evita mostrar un valor equivocado.

**¿Por qué el número desaparece a veces?** Porque el equipo detectó movimiento, un latido irregular o una señal de mala calidad. Prefiere mostrar “--” antes que un valor dudoso.

**¿Por qué la SpO₂ puede salir cerca de 100 %?** La fórmula del equipo es genérica y no se ajustó a este sensor. Es probable que muestre valores más altos que los reales. Por eso no debe usarse para decidir nada.

**¿Qué significa “ESTIM. EXPERIMENTAL” en la parte baja?** Que los valores son estimaciones de un prototipo y no mediciones clínicas. Es el estado normal del pie.

**¿Se guardan mis mediciones?** No. El equipo no guarda ni envía datos.

**¿Puedo medir en el dedo del pie o en la oreja?** No está previsto ni se probó. Use la yema del dedo índice o del medio.

**¿Qué es la luz roja que se ve en el sensor?** Es una de las dos luces que usa el sensor para medir. La otra es infrarroja y no se ve a simple vista.

## 24. Solución de problemas

| Problema | Posible causa | Solución |
| --- | --- | --- |
| No enciende | Cable USB o cargador defectuoso | Pruebe otro cable, otro cargador u otro puerto |
| Pie “SENSOR NO DETECTADO” al encender | El sensor no responde o un cable está suelto | Espere: el equipo lo busca solo cada 5 s. Si sigue, desconecte y conecte el USB; si persiste, que un técnico revise los cables |
| Pie “SENSOR NO RESPONDE” durante la medición | Falso contacto o el sensor dejó de enviar datos | Espere unos 10 s: el equipo intenta recuperarse solo. Si no, siga la sección 16 |
| “SIN DEDO” con el dedo puesto | El dedo no cubre la ventanita o está levantado | Cubra por completo la ventanita con la yema |
| “MALA” | Señal débil o demasiada luz | Siga la sección 15 |
| “BUSCANDO” por más de 30 s | Movimiento, dedo frío o presión excesiva | Retire el dedo, espere “SIN DEDO” y vuelva a colocarlo quieto |
| “REGULAR” todo el tiempo | La señal sirve para el pulso, pero no para la saturación | Relaje la mano, evite moverla y ajuste la presión |
| La gráfica se corta | Se perdieron datos un momento | Normal si ocurre pocas veces; si es frecuente, avise a un técnico |
| “GRAFICA DESACTIVADA” | La gráfica se apagó desde la computadora | Pida que la reactiven (comando l); los números siguen funcionando |
| Calor, olor o humo | Falla eléctrica | Desconecte el USB de inmediato y no vuelva a usarlo |

Si un problema no se resuelve con esta tabla, deje de usar el equipo y comuníquese con el responsable del proyecto: \[nombre y contacto pendientes\].
