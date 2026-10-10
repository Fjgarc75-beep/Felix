#!/usr/bin/env python3
"""
gen_reference_cat1.py - Valores de referencia INDEPENDIENTES para la Categoria 1.

No lee el firmware: parte solo de las especificaciones del diseno
(fs = 100 muestras/s, Butterworth de 2.o orden, pasa-altas 0.5 Hz,
pasa-bajas 5 Hz, DC con tau = 1 s y 0.1 s, envolvente con tau = 2 s,
curva SpO2 = a*R^2 + b*R + c con los coeficientes de Maxim).

Salida: ref_cat1.h (arreglos double) que incluyen las pruebas en C++.
Uso: python3 gen_reference_cat1.py <ruta_de_salida/ref_cat1.h>
"""
import math
import sys

import numpy as np
from scipy import signal

FS = 100.0
HP_FC, LP_FC = 0.5, 5.0
DC_TAU, DC_TAU_FAST, ENV_TAU = 1.0, 0.1, 2.0
A, B, C = -45.060, 30.354, 94.845      # curva de Maxim (especificacion de la Etapa 6.2)

hp_b, hp_a = signal.butter(2, HP_FC, btype="highpass", fs=FS)
lp_b, lp_a = signal.butter(2, LP_FC, btype="lowpass", fs=FS)

n_step, n_sine = 200, 300
t = np.arange(n_sine) / FS
step = np.ones(n_step)
sine12 = np.sin(2 * math.pi * 1.2 * t)
sine15 = np.sin(2 * math.pi * 15.0 * t)

refs = {
    "HP_B": hp_b, "HP_A": hp_a, "LP_B": lp_b, "LP_A": lp_a,
    "HP_STEP": signal.lfilter(hp_b, hp_a, step),
    "HP_SINE12": signal.lfilter(hp_b, hp_a, sine12),
    "LP_STEP": signal.lfilter(lp_b, lp_a, step),
    "LP_SINE12": signal.lfilter(lp_b, lp_a, sine12),
    "LP_SINE15": signal.lfilter(lp_b, lp_a, sine15),
    "SINE12_IN": sine12, "SINE15_IN": sine15,
}

r_values = [0.428, 0.5, 0.337, 0.34, 1.15, 0.0]
spo2 = [A * r * r + B * r + C for r in r_values]

def gain_db(b, a, f):
    _, h = signal.freqz(b, a, worN=[f], fs=FS)
    return 20 * math.log10(abs(h[0]))

scalars = {
    "DC_ALPHA": 1 - math.exp(-1 / (FS * DC_TAU)),
    "DC_ALPHA_FAST": 1 - math.exp(-1 / (FS * DC_TAU_FAST)),
    "HR_ENV_DECAY": math.exp(-1 / (FS * ENV_TAU)),
    "HP_GAIN_DB_FC": gain_db(hp_b, hp_a, HP_FC),
    "LP_GAIN_DB_FC": gain_db(lp_b, lp_a, LP_FC),
}

out = sys.argv[1] if len(sys.argv) > 1 else "ref_cat1.h"
with open(out, "w") as f:
    import scipy
    f.write("// GENERADO por gen_reference_cat1.py (scipy %s, numpy %s). No editar.\n"
            % (scipy.__version__, np.__version__))
    f.write("#pragma once\n")
    for k, v in scalars.items():
        f.write("static const double REF_%s = %.15g;\n" % (k, v))
    for k, arr in refs.items():
        vals = ", ".join("%.15g" % x for x in arr)
        f.write("static const double REF_%s[%d] = {%s};\n" % (k, len(arr), vals))
    f.write("static const double REF_R_VALUES[%d] = {%s};\n" % (len(r_values), ", ".join("%.15g" % x for x in r_values)))
    f.write("static const double REF_SPO2_VALUES[%d] = {%s};\n" % (len(spo2), ", ".join("%.15g" % x for x in spo2)))
print("referencias escritas en", out)
for k, v in scalars.items():
    print("  %-14s = %.9f" % (k, v))
for r, s in zip(r_values, spo2):
    print("  SpO2(R=%.3f) = %.4f" % (r, s))
