#!/usr/bin/env bash
# =====================================================================
#  setup_deps.sh - Descarga las dependencias de terceros del arnés de PC
#
#  Estas dependencias NO se versionan en el repositorio (licencias de
#  terceros y binarios). El script las deja en harness/arduino/:
#
#   1. SparkFun MAX3010x Sensor Library v1.1.2 (la misma que usa el
#      firmware), copiada en libraries/SparkFun_MAX3010x/.
#   2. Fuente clásica 5x7 de Adafruit GFX (v1.12.6), extraída de
#      glcdfont.c a libraries/Adafruit_GFX/src/glcdfont_data.h para la
#      TFT simulada.
#   3. ctags 5.8-arduino11 (el que usa el preprocesador del IDE de
#      Arduino), en tools/ctags/ctags.
#
#  Uso:  ./setup_deps.sh
#  Var.: CTAGS_BIN  ruta a un ctags de Arduino ya instalado (opcional)
#
#  Requisitos: git, python3, curl o wget (solo si hay que descargar ctags).
#  Probado en Linux x86_64. En Windows, usar WSL.
# =====================================================================
set -euo pipefail
cd "$(dirname "$0")"
H="$(pwd)/harness/arduino"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

echo "== 1. SparkFun MAX3010x v1.1.2"
SF="$H/libraries/SparkFun_MAX3010x"
if [ -f "$SF/src/MAX30105.cpp" ]; then
  echo "   ya existe"
else
  git -c advice.detachedHead=false clone --quiet --depth 1 --branch v1.1.2 \
    https://github.com/sparkfun/SparkFun_MAX3010x_Sensor_Library "$TMP/sf"
  mkdir -p "$SF"
  cp -r "$TMP/sf/src" "$SF/"
  cp "$TMP/sf/library.properties" "$TMP/sf/LICENSE.md" "$SF/"
  echo "   copiada en $SF"
fi

echo "== 2. Fuente 5x7 de Adafruit GFX v1.12.6"
FONT="$H/libraries/Adafruit_GFX/src/glcdfont_data.h"
if [ -f "$FONT" ]; then
  echo "   ya existe"
else
  git -c advice.detachedHead=false clone --quiet --depth 1 --branch 1.12.6 \
    https://github.com/adafruit/Adafruit-GFX-Library "$TMP/gfx"
  python3 - "$TMP/gfx/glcdfont.c" "$FONT" <<'EOF'
import re, sys
src = open(sys.argv[1], encoding="utf-8").read()
m = re.search(r"static const unsigned char font\[\] PROGMEM = \{(.*?)\};", src, re.S)
if not m:
    sys.exit("no se encontró la tabla 'font' en glcdfont.c")
with open(sys.argv[2], "w", encoding="utf-8") as f:
    f.write("#pragma once\nstatic const unsigned char gfxfont[] = {" + m.group(1) + "};\n")
EOF
  echo "   generada en $FONT"
fi

echo "== 3. ctags 5.8-arduino11"
CT="$H/tools/ctags/ctags"
if [ -x "$CT" ]; then
  echo "   ya existe"
else
  mkdir -p "$(dirname "$CT")"
  CAND="${CTAGS_BIN:-}"
  if [ -z "$CAND" ]; then
    for d in "$HOME/.arduino15/packages/builtin/tools/ctags"/*/ctags \
             "$HOME/.arduino15/packages/arduino/tools/ctags"/*/ctags; do
      [ -x "$d" ] && CAND="$d" && break
    done
  fi
  if [ -n "$CAND" ]; then
    cp "$CAND" "$CT"
    echo "   copiado de $CAND"
  else
    URL="https://downloads.arduino.cc/tools/ctags-5.8-arduino11-pm-x86_64-pc-linux-gnu.tar.bz2"
    echo "   descargando $URL"
    if command -v curl >/dev/null; then curl -fsSL "$URL" -o "$TMP/ctags.tbz"; else wget -q "$URL" -O "$TMP/ctags.tbz"; fi
    tar -xjf "$TMP/ctags.tbz" -C "$TMP"
    cp "$(find "$TMP" -type f -name ctags | head -1)" "$CT"
  fi
  chmod +x "$CT"
fi

echo "Dependencias listas. Siguiente paso: ./run_tests.sh cat1"
