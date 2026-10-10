#!/usr/bin/env python3
"""
coverage_cat1.py - Cobertura de líneas de la Categoría 1 a partir de gcov (JSON).

Uso: coverage_cat1.py <Etapa9.ino> <gcov.json.gz principal> <gcov.json.gz variante> <dir_reportes>

- Une la ejecución principal y la variante SPO2_DECIMALS = 0 (una línea
  está cubierta si se ejecutó en cualquiera de las dos).
- Informa la cobertura de las 45 funciones de la Categoría 1 y, como
  referencia, la del archivo completo (las categorías 2a-2g aún no existen).
- La cobertura es un OBJETIVO (>= 85 %), no un criterio de aprobación.
"""
import csv
import gzip
import json
import os
import re
import sys

CAT1 = ("correlationFromSums hrParabolicOffset spMedianOf spCvOf oxSpo2FromR hrStateText "
        "oxStateText calidadTexto calidadColor modoTexto campoNombre uiTextoCentrado "
        "formatSpo2Key uiSpo2FieldX uiPctX resetReasonText arranqueTipoTexto isLargeStep "
        "filtersSettled computeFilterCoefficients hrComputeConstants highPass lowPass "
        "hrMeanInterval hrMedianInterval hrComputeCv hrStableBpm hrRefractorySamples "
        "hrTimeoutSamples hrValleyBefore spMedianR spCvR testMeanDc testMeanRaw testAcRms "
        "subMeanPP subMinPP subCvPP activeTestBlock anyTestRunning calidadDesdeEstados "
        "oxQualityFailReason uiPpgValorAPixel ppgMuestra ppgMuestraOk").split()
OBJETIVO = 85.0


def function_ranges(src_lines):
    """Rango de líneas (1-based, inclusivo) de cada función del .ino."""
    text = "\n".join(src_lines)
    ranges = {}
    for m in re.finditer(r'^[A-Za-z_][\w \*]*?\b(\w+)\s*\([^;{]*\)\s*\{', text, re.M | re.S):
        name = m.group(1)
        start = text.count("\n", 0, m.start()) + 1
        depth, i = 0, m.end() - 1
        while True:
            if text[i] == "{":
                depth += 1
            elif text[i] == "}":
                depth -= 1
                if depth == 0:
                    break
            i += 1
        ranges[name] = (start, text.count("\n", 0, i) + 1)
    return ranges


def load_counts(path, ino_name, branches=None):
    """Conteos por línea; si se pasa 'branches', acumula (linea, indice) -> conteo
    de las ramas que no son de excepción (gcov -b)."""
    counts = {}
    with gzip.open(path, "rt") as f:
        data = json.load(f)
    for fe in data["files"]:
        if os.path.basename(fe["file"]) != ino_name:
            continue
        for ln in fe["lines"]:
            n = ln["line_number"]
            counts[n] = max(counts.get(n, 0), ln["count"])
            if branches is not None:
                for k, br in enumerate(b for b in ln.get("branches", []) if not b["throw"]):
                    branches[(n, k)] = branches.get((n, k), 0) + br["count"]
    return counts


def main():
    ino, g_main, g_var, outdir = sys.argv[1:5]
    src = open(ino, encoding="utf-8").read().split("\n")
    ranges = function_ranges(src)
    name = os.path.basename(ino)
    br1, br2 = {}, {}
    c1, c2 = load_counts(g_main, name, br1), load_counts(g_var, name, br2)
    allbr = set(br1) | set(br2)
    brcov = {k for k in allbr if br1.get(k, 0) > 0 or br2.get(k, 0) > 0}
    executable = set(c1) | set(c2)
    covered = {n for n in executable if c1.get(n, 0) > 0 or c2.get(n, 0) > 0}

    rows, uncovered, tot_e, tot_c = [], [], 0, 0
    brows, bunc, tb_e, tb_c = {}, [], 0, 0
    for fn in CAT1:
        a, b = ranges[fn]
        ex = sorted(n for n in executable if a <= n <= b)
        cv = [n for n in ex if n in covered]
        tot_e += len(ex)
        tot_c += len(cv)
        pct = 100.0 * len(cv) / len(ex) if ex else 100.0
        rows.append((fn, a, b, len(ex), len(cv), pct))
        be = sorted(k for k in allbr if a <= k[0] <= b)
        bc = [k for k in be if k in brcov]
        tb_e += len(be)
        tb_c += len(bc)
        brows[fn] = (len(be), len(bc))
        for k in be:
            if k not in brcov:
                bunc.append((fn, k[0], k[1], src[k[0] - 1].strip()))
        for n in ex:
            if n not in covered:
                uncovered.append((fn, n, src[n - 1].strip()))

    total_pct = 100.0 * tot_c / tot_e if tot_e else 0.0
    file_pct = 100.0 * len(covered) / len(executable) if executable else 0.0
    bpct = 100.0 * tb_c / tb_e if tb_e else 100.0

    with open(os.path.join(outdir, "cat1_cobertura.csv"), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["funcion", "linea_inicio", "linea_fin", "lineas_ejecutables", "lineas_cubiertas", "porcentaje",
                    "ramas", "ramas_cubiertas", "porcentaje_ramas"])
        for r in rows:
            be, bc = brows[r[0]]
            w.writerow([r[0], r[1], r[2], r[3], r[4], "%.1f" % r[5], be, bc, "%.1f" % (100.0 * bc / be if be else 100.0)])
    with open(os.path.join(outdir, "cat1_ramas_no_cubiertas.csv"), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["funcion", "linea", "indice_rama", "codigo"])
        for u in bunc:
            w.writerow(u)
    with open(os.path.join(outdir, "cat1_lineas_no_cubiertas.csv"), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["funcion", "linea", "codigo"])
        for u in uncovered:
            w.writerow(u)
    with open(os.path.join(outdir, "cat1_cobertura.md"), "w") as f:
        f.write("Cobertura de lineas (gcov), principal + variante SPO2_DECIMALS = 0. Objetivo >= %.0f %% (no es criterio de aprobacion).\n\n" % OBJETIVO)
        f.write("- 45 funciones de la Categoria 1: **%d de %d lineas = %.1f %%**\n" % (tot_c, tot_e, total_pct))
        f.write("- Archivo completo (referencia; categorias 2a-2g pendientes): %d de %d lineas = %.1f %%\n\n" % (len(covered), len(executable), file_pct))
        f.write("- Ramas (gcov -b, sin ramas de excepcion) de la Categoria 1: **%d de %d = %.1f %%** (informativo)\n\n" % (tb_c, tb_e, bpct))
        f.write("| Funcion | Lineas del .ino | Ejecutables | Cubiertas | % lineas | Ramas | Ramas cubiertas |\n| --- | --- | --- | --- | --- | --- | --- |\n")
        for r in rows:
            f.write("| `%s` | %d-%d | %d | %d | %.1f | %d | %d |\n" % (r + brows[r[0]]))
        f.write("\nLineas no cubiertas: %d\n" % len(uncovered))
        for u in uncovered:
            f.write("- `%s` linea %d: `%s`\n" % u)
        f.write("\nRamas no cubiertas: %d\n" % len(bunc))
        for u in bunc:
            f.write("- `%s` linea %d, rama %d: `%s`\n" % u)

    print("Categoria 1: %d de %d lineas = %.1f %% (objetivo %.0f %%)" % (tot_c, tot_e, total_pct, OBJETIVO))
    print("Archivo completo (referencia): %d de %d lineas = %.1f %%" % (len(covered), len(executable), file_pct))
    print("Lineas no cubiertas en la Categoria 1: %d" % len(uncovered))
    print("Ramas de la Categoria 1 (informativo): %d de %d = %.1f %%" % (tb_c, tb_e, bpct))
    for u in uncovered:
        print("  %s:%d  %s" % u)
    for u in bunc:
        print("  rama %s:%d[%d]  %s" % u)


if __name__ == "__main__":
    main()
