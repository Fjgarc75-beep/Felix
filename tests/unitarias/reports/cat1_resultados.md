| ID | Funcion | Tipo | Resultado | Verificaciones | Caso |
| --- | --- | --- | --- | --- | --- |
| C1_01_N | `correlationFromSums` | N | OK | 2 | R = 2*I + 3 da 1.0; R = -I da -1.0 (100 muestras) |
| C1_01_L | `correlationFromSums` | L | OK | 2 | n = 1 y señal constante (varianza 0) dan 0 |
| C1_01_E | `correlationFromSums` | E | OK | 4 | n = 0, sumas incoherentes (varianza negativa) y n = 1 incoherente dan 0, sin NaN |
| C1_01_L2 | `correlationFromSums` | L | OK | 1 | IR constante con roja variable (varR > 0, varI = 0) da 0 |
| C1_02_N | `hrParabolicOffset` | N | OK | 3 | (1,3,2) = +1/6; (2,3,1) = -1/6; (1,2,1) = 0 |
| C1_02_L | `hrParabolicOffset` | L | OK | 3 | denominador 0 (2,2,2) = 0; (0,1,1.9) se recorta a +0.5 |
| C1_02_E | `hrParabolicOffset` | E | OK | 1 | (1,1,5) no es un máximo: devuelve 0 |
| C1_03_N | `spMedianOf` | N | OK | 3 | [3,1,2] = 2; [4,1,3,2] = 2.5; el arreglo de entrada no cambia |
| C1_03_L | `spMedianOf` | L | OK | 3 | n = 1; n = 64 (1..64) = 32.5; n = 65 usa solo los 64 primeros |
| C1_03_E | `spMedianOf` | E | OK | 2 | n = 0 y n negativo devuelven 0 |
| C1_04_N | `spCvOf` | N | OK | 1 | [1,2,3]: desviación muestral 1, media 2, CV 0.5 |
| C1_04_L | `spCvOf` | L | OK | 2 | n = 2 [2,4] = 0.4714; valores iguales = 0 |
| C1_04_E | `spCvOf` | E | OK | 2 | n < 2 y media <= 0 devuelven 0 (subCvPP devuelve 99 en ese caso: diferencia documentada) |
| C1_05_N | `oxSpo2FromR` | N | OK | 2 | R = 0.428 -> 99.58 %; R = 0.5 -> 98.76 % (referencia Python) |
| C1_05_L | `oxSpo2FromR` | L | OK | 4 | vértice 0.337 -> 99.96; 0.34 -> 99.96; 1.15 -> 70.16 |
| C1_05_E | `oxSpo2FromR` | E | OK | 1 | R = 0 devuelve c = 94.845 (fuera del rango útil; los estados lo filtran) |
| C1_06_N | `hrStateText` | N | OK | 3 | los 3 estados del BPM |
| C1_06_E | `hrStateText` | E | OK | 2 | estados fuera de rango (99, -1) -> SIN SENAL |
| C1_07_N | `oxStateText` | N | OK | 5 | los 5 estados de SpO2, textos distintos |
| C1_07_E | `oxStateText` | E | OK | 2 | fuera de rango (-1, 7) -> SIN SENAL |
| C1_08_N | `calidadTexto` | N | OK | 5 | las 5 categorías confirmadas |
| C1_08_L | `calidadTexto` | L | OK | 5 | todas caben en el campo de 8 caracteres |
| C1_08_E | `calidadTexto` | E | OK | 2 | fuera de rango (-1, 9) -> BUSCANDO |
| C1_09_N | `calidadColor` | N | OK | 5 | verde, amarillo, naranja, gris claro, gris claro |
| C1_09_E | `calidadColor` | E | OK | 2 | fuera de rango -> gris claro |
| C1_10_N | `modoTexto` | N | OK | 3 | los 3 textos del pie |
| C1_10_L | `modoTexto` | L | OK | 3 | todos caben en el campo de 21 caracteres |
| C1_10_E | `modoTexto` | E | OK | 2 | modo desconocido (0, 9) -> ESTIM. EXPERIMENTAL |
| C1_11_N | `campoNombre` | N | OK | 8 | los 4 campos, 7 caracteres cada uno |
| C1_11_E | `campoNombre` | E | OK | 1 | campo fuera de rango (7) -> "Pie    " |
| C1_12_N | `uiTextoCentrado` | N | OK | 1 | ("BUENA", 8) -> " BUENA  " |
| C1_12_L | `uiTextoCentrado` | L | OK | 4 | texto de 8 queda igual; vacío = 8 espacios; ancho 21 |
| C1_12_E | `uiTextoCentrado` | E | OK | 2 | texto más largo que el ancho: truncado, sin escribir después de out[ancho] |
| C1_13_N | `formatSpo2Key` | N | OK | 2 | 985 -> " 98.5"; 1000 -> "100.0" |
| C1_13_L | `formatSpo2Key` | L | OK | 3 | 0 -> "  0.0"; 5 -> "  0.5"; todas de 5 caracteres |
| C1_13_E | `formatSpo2Key` | E | OK | 5 | -1 -> " --.-"; búfer de 6 y de 4 bytes sin desborde (byte centinela) |
| C1_14_N | `uiSpo2FieldX` | N | OK | 2 | con 1 decimal: x = 4 y el campo de 5 caracteres termina antes del % |
| C1_15_N | `uiPctX` | N | OK | 1 | el % está en x = 98 |
| C1_16_N | `resetReasonText` | N | OK | 4 | encendido, reset externo, software, panic |
| C1_16_L | `resetReasonText` | L | OK | 4 | los 3 watchdog y el brownout |
| C1_16_E | `resetReasonText` | E | OK | 2 | motivo desconocido (UNKNOWN, 99) -> "otro" |
| C1_17_N | `arranqueTipoTexto` | N | OK | 3 | arranque, reintento, recuperacion P2 |
| C1_17_E | `arranqueTipoTexto` | E | OK | 1 | tipo desconocido (5) -> arranque |
| C1_18_N | `isLargeStep` | N | OK | 3 | DC 100000: x 110001 y 89999 son escalón; 109999 no |
| C1_18_L | `isLargeStep` | L | OK | 3 | x = 110000 exacto no es escalón (estricto); DC 20000 usa el mínimo de 5000 |
| C1_18_E | `isLargeStep` | E | OK | 3 | DC 0: 4999 no, 5001 sí; los canales son independientes |
| C1_19_N | `filtersSettled` | N | OK | 1 | (primed, 0) -> true |
| C1_19_L | `filtersSettled` | L | OK | 1 | (primed, 1) -> false |
| C1_19_E | `filtersSettled` | E | OK | 1 | (no primed, 0) -> false |
| C1_20_N | `computeFilterCoefficients` | N | OK | 14 | coeficientes = scipy.butter (fs 100); -3.01 dB en fc; ganancia DC 0 y 1 |
| C1_20_L | `computeFilterCoefficients` | L | OK | 2 | alfas del DC: 0.009950 (tau 1 s) y 0.095163 (tau 0.1 s) |
| C1_20_E | `computeFilterCoefficients` | E | OK | 2 | ambos biquads estables (polos dentro del círculo unitario) |
| C1_21_N | `hrComputeConstants` | N | OK | 1 | hrEnvDecay = exp(-1/200) = 0.995012 |
| C1_22_N | `highPass` | N | OK | 2 | escalón y seno de 1.2 Hz iguales a scipy (error relativo <= 1e-4) |
| C1_22_L | `highPass` | L | OK | 3 | canal RED no altera al IR; estado en cero devuelve b0*x |
| C1_22_E | `highPass` | E | OK | 2002 | entrada 262143 sostenida 2000 muestras: finita, acotada y tiende a 0 |
| C1_23_N | `lowPass` | N | OK | 4 | escalón, seno de 1.2 Hz y seno de 15 Hz iguales a scipy; 15 Hz atenuado |
| C1_23_L | `lowPass` | L | OK | 3 | canal RED no altera al IR; estado en cero devuelve b0*x |
| C1_23_E | `lowPass` | E | OK | 2001 | entrada 262143 sostenida: finita y tiende a 262143 |
| C1_24_N | `hrMeanInterval` | N | OK | 1 | [800,1000,1200] -> 1000 ms |
| C1_24_L | `hrMeanInterval` | L | OK | 2 | 1 valor; 8 valores (búfer lleno) |
| C1_24_E | `hrMeanInterval` | E | OK | 1 | historial vacío -> 0 |
| C1_25_N | `hrMedianInterval` | N | OK | 2 | [1200,800,1000] desordenado -> 1000; el historial no cambia |
| C1_25_L | `hrMedianInterval` | L | OK | 1 | 8 valores (par): promedio de los dos centrales = 1050 |
| C1_25_E | `hrMedianInterval` | E | OK | 1 | historial vacío -> 0 |
| C1_26_N | `hrComputeCv` | N | OK | 1 | [800,1000,1200] -> CV 0.2 |
| C1_26_L | `hrComputeCv` | L | OK | 1 | 1 valor -> 0 |
| C1_26_E | `hrComputeCv` | E | OK | 1 | historial vacío -> 0 |
| C1_27_N | `hrStableBpm` | N | OK | 2 | media 1000 ms -> 60 BPM; 500 ms -> 120 BPM |
| C1_27_E | `hrStableBpm` | E | OK | 1 | historial vacío -> 0 |
| C1_28_N | `hrRefractorySamples` | N | OK | 3 | sin historial 30; media 1000 ms 60; media 1500 ms 90 (fs 100) |
| C1_28_L | `hrRefractorySamples` | L | OK | 2 | proporcional a fs: 95 -> 28.5; 105 -> 31.5 |
| C1_28_L2 | `hrRefractorySamples` | L | OK | 2 | con historial corto (media 400 ms: 60 % = 240 ms < 300 ms) gana el mínimo -> 30 |
| C1_29_N | `hrTimeoutSamples` | N | OK | 3 | sin historial 300; media 1000 ms 300 (gana el mínimo de 3 s); 1500 ms 375 |
| C1_29_L | `hrTimeoutSamples` | L | OK | 2 | proporcional a fs: 95 -> 285; 105 -> 315 |
| C1_30_N | `hrValleyBefore` | N | OK | 1 | búfer 1000-1100 con valle de -5 en 1050 -> -5 |
| C1_30_L | `hrValleyBefore` | L | OK | 3 | ventana máxima 159 (incluye pico-159); inicio después del pico; envoltura en 160 |
| C1_30_E | `hrValleyBefore` | E | OK | 2 | con historial (media 500 ms) la ventana se reduce a 55 muestras |
| C1_30_L2 | `hrValleyBefore` | L | OK | 4 | historial largo no amplía la ventana; inicio limitado por hrBufStartIdx; pico < ventana |
| C1_31_N | `spMedianR` | N | OK | 1 | 8 valores de R -> su mediana |
| C1_31_E | `spMedianR` | E | OK | 1 | sin valores -> 0 |
| C1_32_N | `spCvR` | N | OK | 1 | igual a spCvOf sobre el historial |
| C1_32_E | `spCvR` | E | OK | 1 | 1 valor -> 0 |
| C1_33_N | `testMeanDc` | N | OK | 3 | suma/cuenta en los bloques 0, 1 y 2 |
| C1_33_E | `testMeanDc` | E | OK | 1 | cuenta 0 -> 0 |
| C1_34_N | `testMeanRaw` | N | OK | 1 | suma/cuenta del valor crudo |
| C1_34_E | `testMeanRaw` | E | OK | 1 | cuenta 0 -> 0 |
| C1_35_N | `testAcRms` | N | OK | 1 | suma de cuadrados 400 en 4 muestras -> 10 |
| C1_35_E | `testAcRms` | E | OK | 1 | cuenta 0 -> 0 |
| C1_36_N | `subMeanPP` | N | OK | 1 | [100,120,80] -> 100 |
| C1_36_L | `subMeanPP` | L | OK | 1 | 12 subventanas (máximo) -> media de las 12 |
| C1_36_E | `subMeanPP` | E | OK | 1 | 0 subventanas -> 0 |
| C1_37_N | `subMinPP` | N | OK | 1 | [100,120,80] -> 80 |
| C1_37_E | `subMinPP` | E | OK | 1 | 0 subventanas -> 0 |
| C1_38_N | `subCvPP` | N | OK | 1 | [100,120,80] -> CV 0.2 |
| C1_38_L | `subCvPP` | L | OK | 1 | 1 subventana -> 0 |
| C1_38_E | `subCvPP` | E | OK | 1 | media <= 0 -> 99 |
| C1_39_N | `activeTestBlock` | N | OK | 3 | fases de medición -> bloques 0, 1, 2 |
| C1_39_E | `activeTestBlock` | E | OK | 4 | IDLE y fases de espera -> -1 |
| C1_40_N | `anyTestRunning` | N | OK | 16 | las 16 combinaciones de las 4 pruebas |
| C1_41_N | `calidadDesdeEstados` | N | OK | 7 | las 6 filas validadas de la tabla |
| C1_41_L | `calidadDesdeEstados` | L | OK | 4 | DC IR 50000 = hay dedo; 49999 = SIN DEDO; las 60 combinaciones |
| C1_41_E | `calidadDesdeEstados` | E | OK | 1 | sensor no listo -> BUSCANDO aunque los demás estados digan otra cosa |
| C1_42_N | `oxQualityFailReason` | N | OK | 5 | cada criterio fallando solo da su texto |
| C1_42_L | `oxQualityFailReason` | L | OK | 11 | umbrales exactos no fallan; un paso fuera sí; prioridad en el orden del código |
| C1_42_E | `oxQualityFailReason` | E | OK | 1 | todos los criterios cumplen -> "calidad baja" (texto por defecto) |
| C1_43_N | `uiPpgValorAPixel` | N | OK | 2 | valor = centro -> fila 127 (centro de 108-145) |
| C1_43_L | `uiPpgValorAPixel` | L | OK | 2 | centro +- rango/2 -> 108 y 145 |
| C1_43_E | `uiPpgValorAPixel` | E | OK | 2 | fuera de escala se recorta a 108 o 145 |
| C1_44_N | `ppgMuestra` | N | OK | 1 | devuelve -spAcIr[idx % 256] |
| C1_44_L | `ppgMuestra` | L | OK | 3 | 255; 256 cae en la casilla 0; 2^32-1 cae en la 255 |
| C1_45_N | `ppgMuestraOk` | N | OK | 2 | devuelve spOk[idx % 256] |
| C1_45_L | `ppgMuestraOk` | L | OK | 2 | 256 y 2^32-1 respetan la envoltura |
