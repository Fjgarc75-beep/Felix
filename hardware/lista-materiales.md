# Lista de materiales

> Prototipo con el firmware de la Etapa 9 (MD5 `4d70eb1aef89be2d7ab61ec6d39b9f12`). Solo se listan los componentes que constan en la documentación del proyecto. Lo que no está documentado se indica como pendiente; no se añaden componentes supuestos.

| # | Cantidad | Componente | Modelo o especificación | Uso en el prototipo | Estado de la información |
| --- | --- | --- | --- | --- | --- |
| 1 | 1 | Tarjeta de desarrollo ESP32 | ESP32-WROOM-32 (la cabecera del firmware la nombra “NodeMCU ESP-32S”) | Adquisición, procesamiento, interfaz y Monitor Serie | Nombre exacto de la tarjeta por confirmar (I10, F4) |
| 2 | 1 | Módulo sensor óptico | MAX30102 (LED rojo e infrarrojo, fotodiodo, ADC de 18 bits, FIFO de 32), dirección I²C 0x57 | Señales PPG roja e infrarroja | Documentado |
| 3 | 1 | Pantalla TFT | 1.8", 128 × 160, controlador ST7735S, variante `INITR_BLACKTAB` (módulo rotulado “1.8 TFT SPI 128*160 V1.1.1”) | BPM, SpO₂, calidad, pie y gráfica PPG | Documentado |
| 4 | 1 | Condensador electrolítico | 470 µF / 25 V | Reserva de carga en la alimentación | Punto de conexión pendiente (F3) |
| 5 | 1 | Cable USB | Cable de la tarjeta ESP32 | Alimentación y Monitor Serie | Fuente de alimentación concreta pendiente (F3) |
| 6 | — | Cables de conexión y soporte (protoboard o placa) | No documentados | Interconexión | Pendiente (F5: fotografías del montaje) |

**No forman parte del prototipo:** NeoPixel, módulo WiFi o Bluetooth en uso, microSD, servidor ni base de datos (pertenecían a un enfoque inicial abandonado). No consta un condensador de 100 nF (I9).

**Costo:** no documentado.

Ver también: [componentes](componentes.md) y [conexiones](conexiones.md).
