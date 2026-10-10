Prueba de mutacion sobre copias en build/mut/ (el .ino no se modifica). Informativa, no es criterio de aprobacion.

Mutantes eliminados: **16 de 16**

| Id | Funcion | Error introducido | Resultado | Casos que lo detectan |
| --- | --- | --- | --- | --- |
| M01 | `correlationFromSums` | n < 2 -> n < 1 | ELIMINADO | C1_01_E |
| M02 | `isLargeStep` | > -> >= (escalón exactamente en el límite) | ELIMINADO | C1_18_L |
| M03 | `highPass` | signo de a1 invertido | ELIMINADO | C1_22_N C1_22_E |
| M04 | `hrMedianInterval` | mediana par toma solo el elemento superior | ELIMINADO | C1_25_L |
| M05 | `subMinPP` | mínimo -> máximo | ELIMINADO | C1_37_N |
| M06 | `calidadDesdeEstados` | OX_STABILIZING devuelve REGULAR en vez de BUSCANDO | ELIMINADO | C1_41_N C1_41_L |
| M07 | `oxQualityFailReason` | umbral PI: < -> <= | ELIMINADO | C1_42_L |
| M08 | `uiPpgValorAPixel` | eje vertical invertido | ELIMINADO | C1_43_L C1_43_E |
| M09 | `spCvOf` | desviación muestral (n-1) -> poblacional (n) | ELIMINADO | C1_04_N C1_04_L |
| M10 | `uiTextoCentrado` | sin recorte al ancho (desborda el búfer) | ELIMINADO | C1_12_E |
| M11 | `formatSpo2Key` | decimal mal calculado | ELIMINADO | C1_13_N C1_13_E |
| M12 | `activeTestBlock` | bloque 2 devuelve 1 | ELIMINADO | C1_39_N |
| M13 | `oxSpo2FromR` | coeficiente a de la curva alterado (-45.060 -> -45.600) | ELIMINADO | C1_05_N C1_05_L |
| M14 | `hrValleyBefore` | sin límite por hrBufStartIdx | ELIMINADO | C1_30_L2 |
| M15 | `hrRefractorySamples` | fracción adaptativa no aplicada | ELIMINADO | C1_28_N |
| M16 | `testAcRms` | RMS sin raíz cuadrada | ELIMINADO | C1_35_N |
