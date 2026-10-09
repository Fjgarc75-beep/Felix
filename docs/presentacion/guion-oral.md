# Guion oral – Monitor experimental de FC y SpO₂ por PPG

Duración estimada total: ≈ 14 min 25 s (suma de los tiempos por diapositiva). El mismo texto está en las notas del expositor de [presentacion-proyecto.pptx](presentacion-proyecto.pptx).

## 1. Monitor experimental de frecuencia cardiaca y SpO₂ mediante señal PPG (≈ 30 s)

Buenos días. Les presento el monitor experimental de frecuencia cardiaca y SpO₂ que desarrollé para la materia de Sistemas de Información Hospitalaria. Es un prototipo académico construido con un ESP32, un sensor óptico MAX30102 y una pantalla TFT de 1.8 pulgadas. Quiero dejar claro desde el inicio que no es un dispositivo médico: el objetivo es entender y verificar la cadena completa de un monitor de pulso, no sustituir a uno clínico. La versión que voy a describir es el firmware de la Etapa 9, identificado por su MD5.

## 2. Problema (≈ 40 s)

Un oxímetro comercial es una caja cerrada: da un número, pero no deja ver cómo lo calculó ni cuándo deja de ser confiable. Para un ingeniero clínico, entender esa cadena es la base para evaluar equipos y reconocer artefactos. El problema tiene cuatro partes: obtener una señal óptica limpia, que cambia con el tejido y el movimiento; calcular el BPM y la SpO₂; mostrarlos sin interrumpir la lectura, porque el sensor solo guarda unos 320 milisegundos de datos; y detectar cuándo el sensor falla para no mostrar valores congelados. La pregunta del proyecto resume esto: cómo obtener los valores y ocultarlos cuando no son confiables.

## 3. Justificación (≈ 40 s)

¿Por qué hacerlo? Primero, porque hace visibles los pasos que un monitor comercial oculta: la luz, la señal, el pulso, la razón R y el valor final. Segundo, porque obliga a tratar la calidad del dato y su estado de validez, algo central en un sistema de información hospitalaria: un valor sin estado no sirve para un registro clínico. Tercero, porque es barato y repetible. En la cadena de información, este prototipo cubre los dos primeros eslabones: genera el dato y le asigna un estado. El tercero, enviarlo a un sistema de información, no está implementado y lo presento como trabajo futuro.

## 4. Objetivos (≈ 45 s)

El objetivo general es adquirir la señal PPG roja e infrarroja, estimar el BPM y una SpO₂ experimental, evaluar la calidad y mostrar todo en la pantalla, ocultando lo que no sea confiable. Lo dividí en objetivos específicos, y a la derecha indico cómo se verificó cada uno, porque no todos tienen el mismo nivel de evidencia. La adquisición y el filtrado se validaron en el hardware. La SpO₂ también se probó en el hardware, pero solo contra criterios internos, sin un oxímetro de referencia. La robustez de la Etapa 9 solo se ha probado en simulación; sus pruebas físicas están pendientes.

## 5. Arquitectura del sistema (≈ 45 s)

La arquitectura es sencilla. El sensor MAX30102 tiene dos LED, rojo e infrarrojo, un fotodiodo y un convertidor de 18 bits; guarda las muestras en un FIFO de 32. El ESP32 lo lee por I²C a 400 kilohertz y hace todo el trabajo en seis bloques: adquisición cada 10 milisegundos, procesamiento PPG, detección de pulsos, cálculo de R y SpO₂, calidad e interfaz, y la vigilancia que agregué en la Etapa 9. La pantalla recibe los datos por SPI a 16 megahertz. La computadora solo se usa para enviar comandos y leer diagnósticos por el Monitor Serie; no participa en la medición.

## 6. Hardware (≈ 40 s)

El hardware son tres módulos y un condensador. El ESP32-WROOM-32 hace todo el procesamiento. El MAX30102 es el sensor óptico, en la dirección I²C 0x57. La pantalla es una TFT de 1.8 pulgadas con controlador ST7735, que en mi módulo requiere la variante BLACKTAB. Hay un condensador electrolítico de 470 microfaradios como reserva de carga; el punto exacto donde está conectado lo tengo pendiente de documentar. Todo trabaja a 3.3 volts. No uso el pin de interrupción del sensor ni la microSD, y el prototipo no tiene WiFi, Bluetooth, alarmas ni registro.

## 7. Diagrama de conexiones (≈ 40 s)

Estas son las conexiones reales. El sensor va al bus I²C: alimentación de 3.3 volts, tierra, SDA al GPIO21 y SCL al GPIO22; INT y RD quedan sin conectar. La pantalla va al bus SPI: reloj al GPIO18, datos al GPIO23, A0 al 16, reset al 17 y chip select al 5, con VCC y la retroiluminación a 3.3 volts. Un detalle que confunde a cualquiera que arme el circuito: los dos módulos tienen un pin llamado SDA, pero no son lo mismo. En el sensor es la línea de datos de I²C; en la pantalla es la salida de datos de SPI, es decir, MOSI. Las mismas conexiones están escritas en la cabecera del firmware.

## 8. MAX30102 y señal PPG (≈ 45 s)

El MAX30102 trabaja por reflexión: el LED y el fotodiodo están del mismo lado y el dedo se apoya encima. La luz que regresa tiene una parte casi constante, la DC, que viene del tejido y de la sangre que no pulsa, y una parte que oscila con cada latido, la AC. Usamos dos colores, rojo e infrarrojo, porque la hemoglobina oxigenada y la desoxigenada absorben distinto en cada uno. El sensor muestrea a 400 por segundo y promedia de cuatro en cuatro. En la Etapa 4.1 medimos 99.1 muestras por segundo sin pérdidas. El infrarrojo dio casi tres veces más amplitud que el rojo, y la señal con dedo fue 55 veces mayor que sin dedo.

## 9. Procesamiento de señal (≈ 45 s)

Cada muestra recorre la misma cadena. Primero se resta la componente DC, que se estima con una constante de tiempo de un segundo. Después pasa por un pasa-altas de 0.5 hertz, que atenúa la deriva lenta, y un pasa-bajas de 5 hertz, que quita el ruido; ambos son Butterworth de segundo orden. El resultado es la componente AC; la invertimos para que el pico sistólico quede hacia arriba. Si hay un escalón brusco, por ejemplo al poner o quitar el dedo, los filtros se reinician y hay dos segundos de estabilización. En las pruebas en PC, los coeficientes coincidieron con los de scipy con un error menor a una cienmilésima, y la salida de los filtros con un error relativo menor a una diezmilésima.

## 10. Cálculo de BPM (≈ 50 s)

Para detectar pulsos uso tres criterios. Primero, un umbral adaptativo: la mitad de una envolvente que sigue la amplitud de la señal. Segundo, la prominencia: el pico tiene que subir al menos la mitad de la envolvente desde el valle de su propio ciclo; así se descarta la muesca dicrótica. Tercero, un periodo refractario. Cada intervalo se acepta solo si está entre 40 y 180 BPM y no se aleja más de 30 % de la mediana reciente. El BPM mostrado es 60000 entre la media de hasta ocho intervalos. Y solo se muestra en el estado BPM VÁLIDO, que exige cinco intervalos aceptados seguidos con poca variación. La exactitud frente a un conteo manual, la prueba H5, está pendiente.

## 11. Estimación de SpO₂ (≈ 55 s)

Para la SpO₂, en cada latido aceptado calculo la razón R: la pulsatilidad relativa del rojo entre la del infrarrojo. El estimador principal es una regresión entre las dos señales en todo el latido, porque es menos sensible al ruido que el pico a pico; el pico a pico y el RMS quedan como verificación cruzada. Cada latido tiene que cumplir cinco criterios de calidad y dar un R entre 0.34 y 1.15. Se muestra la mediana de los últimos ocho R válidos, y solo si hay al menos cinco y varían menos de 10 %. La conversión usa la curva genérica de Maxim. Y aquí está el punto más importante de la presentación: esos coeficientes no están calibrados para este módulo, así que el número es una estimación experimental, no una medición clínica.

## 12. Interfaz TFT (≈ 45 s)

La pantalla es de 128 por 160 píxeles y usa la distribución B: arriba el BPM en verde, después la SpO₂ con un decimal en cian, luego la calidad de la señal, la onda PPG y un pie que dice "estimación experimental". Para no interrumpir la lectura del sensor, la pantalla nunca se borra completa durante la medición: los estados se revisan cada 250 milisegundos y cada campo se redibuja como máximo dos veces por segundo, solo si cambió. En la Etapa 7.3 el peor caso de dibujo fue de unos 18 milisegundos. Al encender aparece durante tres segundos el aviso de que no es un dispositivo médico.

## 13. Gráfica PPG en tiempo real (≈ 40 s)

La gráfica PPG se agregó en la Etapa 8.2 con una regla: no hacer otra adquisición. Lee el mismo búfer que ya existía para calcular R. Promedia cuatro muestras por columna, así que avanza unas 25 columnas por segundo y recorre la pantalla en unos cinco segundos. La escala se ajusta sola: crece de inmediato si la señal aumenta y se reduce despacio para no saltar. El color indica la calidad: verde cuando es buena o regular, gris cuando se está buscando, y una línea plana cuando no hay dedo. Dibujar una columna tomó 142 microsegundos en la Etapa 7.1, pero la velocidad real del barrido en la versión actual todavía está pendiente de medir.

## 14. Robustez P1–P4 (≈ 55 s)

La Etapa 9 resolvió un problema que antes pasaba inadvertido: si el sensor dejaba de enviar muestras, la pantalla se quedaba con el último BPM y la última SpO₂, como si siguieran vigentes. Agregué cuatro mejoras sin tocar los algoritmos validados. P1: si pasa un segundo sin muestras, se ocultan los valores y aparece "SENSOR NO RESPONDE". P2: a los tres segundos se reinicializa el sensor, sin reiniciar el microcontrolador. P3 registra el arranque por separado, para saber si las pérdidas ocurren al encender o durante la operación. P4 usa sumas de 64 bits para pruebas largas. La imagen muestra la secuencia, pero es una simulación en PC: la prueba física H10, desconectando el cable de datos, está pendiente.

## 15. Metodología de pruebas (≈ 45 s)

El plan de pruebas de la Etapa 10 tiene tres niveles. El primero son pruebas unitarias en la computadora: las 45 funciones de cálculo, comparadas con valores que calculé por separado en Python. Ese nivel ya está ejecutado y aprobaron los 116 casos. El segundo son pruebas de integración en la computadora, con simuladores del sensor y de la pantalla; están definidas pero pendientes. El tercero son 14 pruebas físicas con el hardware real, también pendientes. Hay dos reglas: ninguna prueba modifica el firmware, y lo compruebo con el MD5 antes y después; y la cobertura de 85 % es un objetivo, no un requisito para aprobar.

## 16. Resultados (≈ 60 s)

Estos son los resultados reales, separados por tipo. En hardware, la prueba de señal de la Etapa 4.1 dio 99.1 muestras por segundo, sin pérdidas, con una correlación entre rojo e infrarrojo de 0.98. La prueba de SpO₂ de la 6.2 dio 35 latidos válidos, con R estable, CV de 4 %, pero una SpO₂ media de 99.9 %. Ese valor está en la zona plana de la curva y sugiere que la curva sobreestima para este módulo; por eso lo marco con observaciones. La pantalla dibuja en unos 18 milisegundos en el peor caso. En la 7.3 hubo errores I²C al principio y quedaron 14 pérdidas acumuladas cuyo origen está pendiente. En PC, la Categoría 1 aprobó completa. Y lo más importante: la versión actual, la Etapa 9, todavía no tiene pruebas físicas.

## 17. Lo que este prototipo no es (≈ 45 s)

Quiero ser explícito con las limitaciones. Este prototipo no es un dispositivo médico certificado y no sustituye a un monitor clínico. La SpO₂ no tiene validación clínica: la curva es genérica y no la comparé con un oxímetro de referencia. Por lo tanto, la exactitud clínica no está demostrada, ni para la SpO₂ ni para el BPM, y el equipo no debe usarse para diagnóstico. En lo técnico, la versión actual no tiene pruebas físicas, los filtros usan la frecuencia nominal y no hay alarmas ni registro. El valor del proyecto está en la adquisición, el procesamiento, la visualización y la validación experimental.

## 18. Conclusiones (≈ 45 s)

Para cerrar. Construí la cadena completa de un monitor de pulso, del sensor a la pantalla, con un firmware de 154 funciones en el que cada capacidad se puede rastrear hasta su línea de código. La adquisición y la señal PPG quedaron validadas en el hardware. La razón R es estable en reposo, pero la SpO₂ que se deriva de ella sigue siendo una estimación experimental. La pantalla no interfiere con la lectura y, sobre todo, oculta los valores cuando no son confiables. Y, con honestidad: el proyecto no está completamente validado, porque las pruebas físicas de la versión actual siguen pendientes.

## 19. Trabajo futuro (≈ 40 s)

El trabajo futuro tiene tres líneas. La primera es cerrar la validación: ejecutar las 14 pruebas físicas y las pruebas de integración en la computadora. La segunda es mejorar la medición: calibrar la curva de SpO₂ contra un oxímetro de referencia, recalcular los filtros con la frecuencia medida y controlar la corriente de los LED automáticamente. La tercera conecta con la materia: enviar el valor, su calidad y su estado a un sistema de información hospitalaria, por ejemplo con HL7 FHIR, que hoy no está implementado. Cualquiera de estos cambios exigiría una nueva versión del firmware y repetir su validación.

## 20. Preguntas (≈ 15 s)

Muchas gracias. Con esto termino. La documentación técnica, el manual de usuario, el plan de pruebas y el expediente de evidencias están disponibles para su revisión. Quedo atento a sus preguntas.

Preguntas probables y respuesta corta:
1) ¿Qué tan exacta es la SpO₂? No se ha demostrado; no hubo oxímetro de referencia y la curva es genérica.
2) ¿Por qué 99.9 %? R ≈ 0.375 cae en la zona plana de la curva; el firmware advierte que probablemente sobreestima.
3) ¿Qué pasa si se mueve el dedo? Un escalón reinicia los filtros; el BPM vuelve a ESTABILIZANDO y no se muestra un valor dudoso.
4) ¿Qué falta para validarlo? Las 14 pruebas físicas H0–H13 y las de integración en PC.
