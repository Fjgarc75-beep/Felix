# Firmware

| Carpeta | Contenido |
| --- | --- |
| [`etapa9-validada/Etapa9/Etapa9.ino`](etapa9-validada/Etapa9/Etapa9.ino) | **Línea base actual.** MD5 `4d70eb1aef89be2d7ab61ec6d39b9f12`, 4112 líneas, 154 funciones |
| [`etapa9-validada/Etapa9_vs_Etapa8_2.diff`](etapa9-validada/Etapa9_vs_Etapa8_2.diff) | Cambios de la Etapa 9 (P1–P4) respecto a la 8.2 |
| [`etapa9-validada/Etapa9_correccionP3.diff`](etapa9-validada/Etapa9_correccionP3.diff) | Corrección autorizada del registro de arranque (P3) |
| [`historial/`](historial/) | Firmwares de las etapas 1 a 8.2, solo como historia del diseño |

> **Sobre el nombre “etapa9-validada”.** Es el nombre de la línea base aprobada para la Etapa 10. La Etapa 9 **todavía no está validada en hardware**: sus pruebas físicas H0–H13 están pendientes de ejecución ([resultados](../docs/pruebas/resultados-pruebas.md)).

El sketch está dentro de una subcarpeta `Etapa9/` porque el IDE de Arduino exige que la carpeta se llame igual que el archivo `.ino`.

## Verificar la versión

```bash
md5sum firmware/etapa9-validada/Etapa9/Etapa9.ino
# 4d70eb1aef89be2d7ab61ec6d39b9f12
```

En Windows (PowerShell): `Get-FileHash -Algorithm MD5 firmware\etapa9-validada\Etapa9\Etapa9.ino`.

Cualquier cambio en el archivo produce otro MD5 y exige una nueva validación.

## Compilar y cargar

1. Instale el IDE de Arduino y el núcleo **Arduino-ESP32 3.3.x** de Espressif (Gestor de tarjetas → “esp32”). La cabecera del firmware indica esa versión; la versión exacta usada en el prototipo está pendiente de registrar (F9).
2. Instale estas librerías desde el Gestor de librerías:
   - SparkFun MAX3010x Pulse and Proximity Sensor Library;
   - Adafruit ST7735 and ST7789 Library;
   - Adafruit GFX Library (y Adafruit BusIO, que pide como dependencia).
3. Abra `firmware/etapa9-validada/Etapa9/Etapa9.ino`.
4. Seleccione la placa ESP32 que corresponda a su tarjeta y el puerto serie. La configuración exacta de placa usada en el prototipo no está documentada (F9).
5. Compile y cargue. Abra el Monitor Serie a **115200 baudios**; el comando `h` muestra la ayuda.

Conexiones: [hardware/conexiones.md](../hardware/conexiones.md).

## Historial

| Etapa | Archivo | MD5 | Estado declarado |
| --- | --- | --- | --- |
| 8.2 | `historial/Etapa8_2/Etapa8_2.ino` | `f9c9c57b769c5505509531b88b9f9d4d` | Validada según la cabecera de la Etapa 9; sin cifras de hardware archivadas |
| 7.3 | `historial/Etapa7_3/Etapa7_3.ino` | `c3bb33d0e216a8bcaa341d23ccbcf18f` | Validada por el autor, con observaciones |
| 7.2 | `historial/Etapa7_2_TFT/Etapa7_2_TFT.ino` | `d0e9385c39682f96324fcdad56b46eeb` | Validada por el autor (valores simulados en pantalla) |
| 7.1 | `historial/Etapa7_1_TFT/Etapa7_1_TFT.ino` | `245bc2e6a458e25bf56b9b417d1cc1f8` | Validada por el autor |
| 6.2 | `historial/Etapa6_2_SpO2/Etapa6_2_SpO2.ino` | `ea2d0a065323784b63b60efae79229bc` | Validada por el autor, con observaciones |
| 6.1 | `historial/Etapa6_1_R/Etapa6_1_R.ino` | `5e5204aea757fc83c9e174b6c83d2200` | Sin reporte propio; incluida en la 6.2 |
| 5 | `historial/Etapa5_BPM/Etapa5_BPM.ino` | `9dd9f4e032f1dbf0a28432339c813e0c` | Sin reporte propio; incluida en la 6.2 |
| 4.1 | `historial/Etapa4_1_PPG/Etapa4_1_PPG.ino` | `0eb985309c7991450c377f8e88ce9ea2` | Validada por el autor |
| 4 | `historial/Etapa4_PPG/Etapa4_PPG.ino` | `35bca706ee1a85f61dab3f42cda1befc` | Sustituida por la 4.1 |
| 3 | `historial/Etapa3_RED_IR/Etapa3_RED_IR.ino` | `189e30e5b7aaf5d6f40cfd3053316a88` | Validada por el autor, sin cifras |
| 2.1 | `historial/Etapa2_1_I2C_MAX30102/Etapa2_1_I2C_MAX30102.ino` | `e490a7cba7b88158deda5dbfa258af64` | Validada por el autor, sin cifras |
| 2 | `historial/Etapa2_I2C_MAX30102/Etapa2_I2C_MAX30102.ino` | `d1de203df1fa0cf14581092de80fd534` | Validada por el autor, sin cifras |
| 1 | `historial/Etapa1_ESP32_basico/Etapa1_ESP32_basico.ino` | `7f838ba17bc0453d56a3eaf20b51148c` | Validada por el autor, sin cifras |

> El firmware es un prototipo académico y experimental. No es un dispositivo médico certificado y no debe usarse para diagnóstico ni para decisiones de salud.
