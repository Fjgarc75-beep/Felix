# Pruebas – Etapa 10

Las pruebas siguen el [plan de pruebas de la Etapa 10](../docs/pruebas/plan-pruebas-etapa10.md). Ninguna prueba modifica el firmware: cada corrida verifica el MD5 de `Etapa9.ino` (`4d70eb1aef89be2d7ab61ec6d39b9f12`) antes y después.

| Carpeta | Nivel | Entorno | Estado |
| --- | --- | --- | --- |
| [`unitarias/`](unitarias/) | Categoría 1: 45 funciones puras o deterministas | PC (simulación) | **Ejecutada:** 116 de 116 casos aprobados; líneas 257/257 y ramas 200/200; mutación 16/16 |
| [`integracion/`](integracion/) | Categorías 2a–2g (109 funciones, incluidas `setup()` y `loop()` en los flujos) y flujos E1–E17 | PC con simuladores | **Pendiente de ejecución** |
| [`hardware/`](hardware/) | H0–H13 sobre el ESP32, el MAX30102 y la TFT reales | Hardware | **Pendiente de ejecución** |

Reglas:

- Los resultados de PC son **simulación** y no sustituyen a las pruebas físicas.
- La cobertura gcov ≥ 85 % es un **objetivo**, no un criterio de aprobación, salvo instrucción explícita posterior del profesor.
- Una prueba solo se marca como aprobada si existe evidencia ([expediente de evidencias](../docs/evidencias/README.md)).

Resultados consolidados: [docs/pruebas/resultados-pruebas.md](../docs/pruebas/resultados-pruebas.md).
