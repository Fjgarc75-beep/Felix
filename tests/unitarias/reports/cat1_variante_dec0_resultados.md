| ID | Funcion | Tipo | Resultado | Verificaciones | Caso |
| --- | --- | --- | --- | --- | --- |
| C1_13_V0 | `formatSpo2Key` | L | OK | 4 | modo entero (SPO2_DECIMALS = 0): 98 -> " 98"; -1 -> " --" |
| C1_14_V0 | `uiSpo2FieldX` | L | OK | 2 | modo entero: x = 40 y el campo de 3 caracteres termina antes del % |
| C1_15_V0 | `uiPctX` | L | OK | 1 | modo entero: el % sigue en x = 98 |
