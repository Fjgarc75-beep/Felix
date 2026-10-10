# Componentes de hardware

> Versión documentada: `firmware/etapa9-validada/Etapa9/Etapa9.ino`, MD5 `4d70eb1aef89be2d7ab61ec6d39b9f12`. Exportado al repositorio el 2026-10-09.

| Componente | Modelo | Función | Interfaz | Alimentación |
| --- | --- | --- | --- | --- |
| Microcontrolador | ESP32-WROOM-32 en tarjeta de desarrollo (la cabecera del firmware la nombra “NodeMCU ESP-32S”; confirmar: I10) | Adquisición, procesamiento, interfaz y Monitor Serie | USB-serie a 115200 baudios; I2C; SPI | USB de la tarjeta \[pendiente de confirmar: F3\] |
| Sensor óptico | MAX30102 (módulo; LED rojo e IR, fotodiodo, ADC de 18 bits, FIFO de 32) | Señales PPG RED e IR | I2C, dirección 0x57, 400 kHz | 3.3 V desde el ESP32 (VIN) |
| Pantalla | TFT “1.8 TFT SPI 128\*160 V1.1.1”, controlador ST7735S, variante `INITR_BLACKTAB` | BPM, SpO₂, calidad, pie y gráfica PPG | SPI por hardware (VSPI), 16 MHz | 3.3 V (VCC y LED de retroiluminación) |
| Condensador | Electrolítico 470 µF / 25 V | Reserva de carga en la alimentación | — | Ubicación en el circuito \[pendiente: F3\] |

No consta que el prototipo incluya el condensador de 100 nF que mencionan los archivos del enfoque inicial; NeoPixel, WiFi y servidor no forman parte del prototipo. Ver la inconsistencia I9 del inventario.

## Alimentación eléctrica

Todo el sistema funciona a 3.3 V. El MAX30102 (VIN) y la TFT (VCC y LED) se alimentan del pin de 3.3 V de la tarjeta del ESP32, cuyo regulador se alimenta a su vez por el conector USB. El USB es también la vía del Monitor Serie con el que se envían los comandos y se leen los resultados. La fuente concreta (puerto de computadora, cargador u otra) no está documentada \[pendiente: F3\].

El prototipo incluye un condensador electrolítico de 470 µF / 25 V. Su función habitual en un montaje así es servir de reserva de carga frente a los picos de corriente de los LED y de la retroiluminación. Los archivos del enfoque inicial lo ubicaban en el riel de 3.3 V, pero el nodo real no está documentado \[pendiente: F3\]. Su tensión nominal de 25 V supera con amplio margen la del circuito.

El firmware no mide la tensión de alimentación. Lo que sí hace es informar el motivo del último reinicio, incluido “CAIDA DE VOLTAJE (brownout)” (`resetReasonText`, L3612–3624, salida de `y`). La prueba H12 (60 min, motivo de reinicio = encendido) está pendiente de ejecución.

La corriente de los LED del sensor es de 6.2 mA por defecto (0x1F × 0.2 mA, L173). Se puede variar a mano con `+` y `-` en pasos de 1 mA, entre 1.2 y 15.2 mA (inconsistencia I2).

Detalle completo: capítulos 13–16 de la [documentación técnica](../docs/documentacion-tecnica.md).
