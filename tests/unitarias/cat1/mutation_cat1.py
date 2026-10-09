#!/usr/bin/env python3
"""
mutation_cat1.py - Prueba de mutación (control de sensibilidad) de la Categoría 1.

Introduce UN error a propósito en una COPIA del firmware preprocesado
(build/Etapa9.ino.cpp -> build/mut/<id>/Etapa9.ino.cpp), recompila las pruebas
y comprueba que al menos un caso falla ("mutante eliminado"). El .ino original
nunca se toca: el script verifica su MD5 al principio y al final.

Uso: mutation_cat1.py <tests_root> <Etapa9.ino original> <md5_esperado> <dir_reportes>
"""
import hashlib
import os
import shutil
import subprocess
import sys

# (id, función, descripción, texto original exacto, texto mutado)
MUTANTES = [
    ("M01", "correlationFromSums", "n < 2 -> n < 1",
     "if (n < 2.0) return 0.0f;", "if (n < 1.0) return 0.0f;"),
    ("M02", "isLargeStep", "> -> >= (escalón exactamente en el límite)",
     "return fabsf(x - dc) > limit;", "return fabsf(x - dc) >= limit;"),
    ("M03", "highPass", "signo de a1 invertido",
     "hpZ1[ch] = hpB1 * x - hpA1 * y + hpZ2[ch];", "hpZ1[ch] = hpB1 * x + hpA1 * y + hpZ2[ch];"),
    ("M04", "hrMedianInterval", "mediana par toma solo el elemento superior",
     "return 0.5f * (tmp[hrHistCount / 2 - 1] + tmp[hrHistCount / 2]);", "return tmp[hrHistCount / 2];"),
    ("M05", "subMinPP", "mínimo -> máximo",
     "if (subPP[i][ch] < m) m = subPP[i][ch];", "if (subPP[i][ch] > m) m = subPP[i][ch];"),
    ("M06", "calidadDesdeEstados", "OX_STABILIZING devuelve REGULAR en vez de BUSCANDO",
     "if (oxState == OX_STABILIZING) return CAL_BUSCANDO;", "if (oxState == OX_STABILIZING) return CAL_REGULAR;"),
    ("M07", "oxQualityFailReason", "umbral PI: < -> <=",
     "if (spLastPiIr < SP_MIN_PI_IR)", "if (spLastPiIr <= SP_MIN_PI_IR)"),
    ("M08", "uiPpgValorAPixel", "eje vertical invertido",
     "int y = (int)lroundf(mid - (valor - uiPpgCenter) * alto / uiPpgSpan);",
     "int y = (int)lroundf(mid + (valor - uiPpgCenter) * alto / uiPpgSpan);"),
    ("M09", "spCvOf", "desviación muestral (n-1) -> poblacional (n)",
     "return sqrtf(s / (n - 1)) / mean;", "return sqrtf(s / n) / mean;"),
    ("M10", "uiTextoCentrado", "sin recorte al ancho (desborda el búfer)",
     "if (n > ancho) n = ancho;", ";"),
    ("M11", "formatSpo2Key", "decimal mal calculado",
     'snprintf(buf, bufLen, "%3d.%d", key / 10, key % 10);', 'snprintf(buf, bufLen, "%3d.%d", key / 10, key % 9);'),
    ("M12", "activeTestBlock", "bloque 2 devuelve 1",
     "if (testPhase == TEST_REMOVE_MEASURE)   return 2;", "if (testPhase == TEST_REMOVE_MEASURE)   return 1;"),
    ("M13", "oxSpo2FromR", "coeficiente a de la curva alterado (-45.060 -> -45.600)",
     "const float    OX_COEF_A             = -45.060f;", "const float    OX_COEF_A             = -45.600f;"),
    ("M14", "hrValleyBefore", "sin límite por hrBufStartIdx",
     "if (start < hrBufStartIdx) start = hrBufStartIdx;", "if (start < hrBufStartIdx) start = start;"),
    ("M15", "hrRefractorySamples", "fracción adaptativa no aplicada",
     "if (adaptive > ms) ms = adaptive;\n  }\n  return ms * measuredFs / 1000.0f;\n}\n\nfloat hrTimeoutSamples",
     "if (adaptive > ms) ms = ms;\n  }\n  return ms * measuredFs / 1000.0f;\n}\n\nfloat hrTimeoutSamples"),
    ("M16", "testAcRms", "RMS sin raíz cuadrada",
     "? sqrtf((float)(tAcSumSq[k][ch] / tAcCount[k])) : 0.0f;", "? ((float)(tAcSumSq[k][ch] / tAcCount[k])) : 0.0f;"),
]


def md5(path):
    return hashlib.md5(open(path, "rb").read()).hexdigest()


def main():
    root, ino, expected, reports = sys.argv[1:5]
    if md5(ino) != expected:
        print("ERROR: el firmware no es la linea base"); sys.exit(3)
    build = os.path.join(root, "build")
    base_cpp = open(os.path.join(build, "Etapa9.ino.cpp"), encoding="utf-8").read()
    H = os.path.join(root, "harness", "arduino")
    inc = ["-I", f"{H}/hardware/fake/esp32/cores/fake", "-I", f"{H}/hardware/fake/esp32/libraries/Wire/src",
           "-I", f"{H}/libraries/SPI/src", "-I", f"{H}/libraries/Adafruit_GFX/src",
           "-I", f"{H}/libraries/Adafruit_ST7735/src", "-I", f"{H}/libraries/SparkFun_MAX3010x/src",
           "-I", os.path.join(root, "framework"), "-I", build]
    libobjs = [os.path.join(build, "cat1", o) for o in
               ("test_support.o", "Wire.o", "SPI.o", "stubdump.o", "MAX30105.o")]
    rows = []
    for mid, fn, desc, old, new in MUTANTES:
        n = base_cpp.count(old)
        if n != 1:
            rows.append((mid, fn, desc, "NO APLICADO (%d coincidencias)" % n, "")); continue
        d = os.path.join(build, "mut", mid)
        os.makedirs(d, exist_ok=True)
        open(os.path.join(d, "Etapa9.ino.cpp"), "w", encoding="utf-8").write(base_cpp.replace(old, new))
        rep = os.path.join(d, "rep"); os.makedirs(rep, exist_ok=True)
        cc = ["g++", "-std=gnu++17", "-DARDUINO=10819", "-O0", "-w", "-I", d] + inc + \
             ['-DREPORT_DIR="%s"' % rep, "-c", os.path.join(root, "cat1", "test_cat1.cpp"),
              "-o", os.path.join(d, "t.o")]
        r = subprocess.run(cc, capture_output=True, text=True)
        if r.returncode != 0:
            rows.append((mid, fn, desc, "NO COMPILA", r.stderr[:200])); continue
        subprocess.run(["g++", os.path.join(d, "t.o")] + libobjs + ["-o", os.path.join(d, "t")], check=True)
        r = subprocess.run([os.path.join(d, "t")], capture_output=True, text=True, cwd=d)
        fallidos = [l.split()[1] for l in r.stdout.splitlines() if l.startswith("[FALLA]")]
        estado = "ELIMINADO" if r.returncode != 0 else "SOBREVIVE"
        rows.append((mid, fn, desc, estado, " ".join(fallidos)))
    if md5(ino) != expected:
        print("ERROR: el firmware cambio"); sys.exit(5)
    elim = sum(1 for r in rows if r[3] == "ELIMINADO")
    with open(os.path.join(reports, "cat1_mutacion.md"), "w") as f:
        f.write("Prueba de mutacion sobre copias en build/mut/ (el .ino no se modifica). "
                "Informativa, no es criterio de aprobacion.\n\n")
        f.write("Mutantes eliminados: **%d de %d**\n\n" % (elim, len(rows)))
        f.write("| Id | Funcion | Error introducido | Resultado | Casos que lo detectan |\n| --- | --- | --- | --- | --- |\n")
        for r in rows:
            f.write("| %s | `%s` | %s | %s | %s |\n" % r)
    for r in rows:
        print("%s %-22s %-10s %s" % (r[0], r[1], r[3], r[4]))
    print("Mutantes eliminados: %d de %d" % (elim, len(rows)))


if __name__ == "__main__":
    main()
