#!/usr/bin/env bash
# =====================================================================
#  run_tests.sh - Pruebas de la Etapa 10 (por ahora: Categoría 1)
#
#  Uso:   ./run_tests.sh [cat1]
#  Var.:  ARDUINO_CLI  ruta a arduino-cli (por defecto: arduino-cli en PATH)
#         FIRMWARE     ruta al .ino     (por defecto: ../../firmware/etapa9-validada/Etapa9/Etapa9.ino)
#  Antes de la primera corrida: ./setup_deps.sh (dependencias de terceros).
#
#  Garantías:
#   - El firmware NO se modifica: se verifica su MD5 antes y después.
#   - Todo lo generado queda en build/ y reports/.
#   - La cobertura (gcov) es un OBJETIVO (>= 85 %), no un criterio de
#     aprobación; el script solo falla si falla un caso o cambia el MD5.
# =====================================================================
set -euo pipefail
cd "$(dirname "$0")"
ROOT="$(pwd)"
SUITE="${1:-cat1}"
ARDUINO_CLI="${ARDUINO_CLI:-arduino-cli}"
FIRMWARE="${FIRMWARE:-$ROOT/../../firmware/etapa9-validada/Etapa9/Etapa9.ino}"
EXPECTED_MD5="4d70eb1aef89be2d7ab61ec6d39b9f12"
H="$ROOT/harness/arduino"
BUILD="$ROOT/build"
REPORTS="$ROOT/reports"

md5_of() { md5sum "$1" | cut -d' ' -f1; }

if [ "$SUITE" != "cat1" ]; then echo "Suite no implementada todavia: $SUITE"; exit 2; fi

for dep in "$H/libraries/SparkFun_MAX3010x/src/MAX30105.cpp" "$H/libraries/Adafruit_GFX/src/glcdfont_data.h" "$H/tools/ctags/ctags"; do
  [ -e "$dep" ] || { echo "Falta $dep. Ejecute primero ./setup_deps.sh"; exit 2; }
done

echo "== 1. Linea base"
MD5_BEFORE="$(md5_of "$FIRMWARE")"
echo "   $FIRMWARE"
echo "   MD5 $MD5_BEFORE (esperado $EXPECTED_MD5)"
[ "$MD5_BEFORE" = "$EXPECTED_MD5" ] || { echo "   ERROR: el firmware no es la linea base"; exit 3; }

rm -rf "$BUILD" "$REPORTS"; mkdir -p "$BUILD/firmware/Etapa9" "$BUILD/arduino-data" "$BUILD/cat1" "$BUILD/cat1_dec0" "$REPORTS"
cp "$FIRMWARE" "$BUILD/firmware/Etapa9/Etapa9.ino"

echo "== 2. Preprocesado con arduino-cli (mismo preprocesador que el IDE)"
"$ARDUINO_CLI" version | sed 's/^/   /'
# Se usa la compilacion normal de arduino-cli y se toma el .cpp preprocesado de
# su carpeta de compilacion (la salida de --preprocess mezcla mensajes de red).
ARDUINO_DIRECTORIES_USER="$H" ARDUINO_DIRECTORIES_DATA="$BUILD/arduino-data" \
  "$ARDUINO_CLI" compile --fqbn fake:esp32:fake --libraries "$H/libraries" \
  --build-path "$BUILD/arduino-build" "$BUILD/firmware/Etapa9" > "$BUILD/preprocess.log" 2>&1 \
  || { grep -v "Error initializing\|Downloading" "$BUILD/preprocess.log"; exit 4; }
cp "$BUILD/arduino-build/sketch/Etapa9.ino.cpp" "$BUILD/Etapa9.ino.cpp"
grep -q "^void setup();" "$BUILD/Etapa9.ino.cpp" || { echo "   ERROR: preprocesado sin prototipos"; exit 4; }
echo "   $(grep -cE '^[a-zA-Z].*\);$' "$BUILD/Etapa9.ino.cpp") prototipos generados"

# Variante solo para pruebas: SPO2_DECIMALS = 0 (copia en build/, el firmware no cambia)
sed 's/^const int      SPO2_DECIMALS        = 1;/const int      SPO2_DECIMALS        = 0;/' \
  "$BUILD/Etapa9.ino.cpp" > "$BUILD/Etapa9_dec0.ino.cpp"
grep -q "SPO2_DECIMALS        = 0;" "$BUILD/Etapa9_dec0.ino.cpp" || { echo "   ERROR: no se pudo crear la variante dec0"; exit 4; }

echo "== 3. Referencias independientes (Python/scipy)"
python3 "$ROOT/reference/gen_reference_cat1.py" "$BUILD/ref_cat1.h" | sed 's/^/   /'

echo "== 4. Compilacion con cobertura (g++ --coverage -O0)"
INC=(-I "$H/hardware/fake/esp32/cores/fake" -I "$H/hardware/fake/esp32/libraries/Wire/src"
     -I "$H/libraries/SPI/src" -I "$H/libraries/Adafruit_GFX/src" -I "$H/libraries/Adafruit_ST7735/src"
     -I "$H/libraries/SparkFun_MAX3010x/src" -I "$ROOT/framework" -I "$BUILD")
LIBSRC=("$ROOT/harness/test_support.cpp" "$H/hardware/fake/esp32/libraries/Wire/src/Wire.cpp"
        "$H/libraries/SPI/src/SPI.cpp" "$H/libraries/Adafruit_ST7735/src/stubdump.cpp"
        "$H/libraries/SparkFun_MAX3010x/src/MAX30105.cpp")
build_one() {   # $1 = carpeta, $2.. = banderas extra
  local OUT="$1"; shift
  ( cd "$OUT"
    g++ -std=gnu++17 -DARDUINO=10819 -O0 -g --coverage -w "${INC[@]}" "$@" -DREPORT_DIR="\"$REPORTS\"" \
        -c "$ROOT/cat1/test_cat1.cpp" -o test_cat1.o
    for f in "${LIBSRC[@]}"; do g++ -std=gnu++17 -DARDUINO=10819 -O0 -w "${INC[@]}" -c "$f" -o "$(basename "$f" .cpp).o"; done
    g++ --coverage *.o -o test_cat1 )
}
build_one "$BUILD/cat1"
build_one "$BUILD/cat1_dec0" -DCAT1_VARIANTE_DEC0 -DFIRMWARE_CPP='"Etapa9_dec0.ino.cpp"'
echo "   ejecutables listos"

echo "== 5. Ejecucion"
set +e
( cd "$BUILD/cat1" && ./test_cat1 ) | tee "$REPORTS/cat1_consola.txt"; R1=${PIPESTATUS[0]}
( cd "$BUILD/cat1_dec0" && ./test_cat1 ) | tee "$REPORTS/cat1_variante_dec0_consola.txt"; R2=${PIPESTATUS[0]}
set -e

echo "== 6. Cobertura (objetivo >= 85 %, no criterio de aprobacion)"
( cd "$BUILD/cat1" && gcov --json-format -b -o . test_cat1.o > /dev/null 2>&1 )
( cd "$BUILD/cat1_dec0" && gcov --json-format -b -o . test_cat1.o > /dev/null 2>&1 )
python3 "$ROOT/cat1/coverage_cat1.py" "$BUILD/firmware/Etapa9/Etapa9.ino" \
  "$BUILD/cat1/test_cat1.gcov.json.gz" "$BUILD/cat1_dec0/test_cat1.gcov.json.gz" "$REPORTS" | sed 's/^/   /'

echo "== 7. Prueba de mutacion (informativa; copias en build/mut/, el .ino no cambia)"
python3 "$ROOT/cat1/mutation_cat1.py" "$ROOT" "$FIRMWARE" "$EXPECTED_MD5" "$REPORTS" | sed 's/^/   /'

echo "== 8. Linea base despues de las pruebas"
MD5_AFTER="$(md5_of "$FIRMWARE")"
echo "   MD5 $MD5_AFTER"
[ "$MD5_AFTER" = "$EXPECTED_MD5" ] || { echo "   ERROR: el firmware cambio durante las pruebas"; exit 5; }
printf "md5_antes=%s\nmd5_despues=%s\n" "$MD5_BEFORE" "$MD5_AFTER" > "$REPORTS/cat1_linea_base.txt"

if [ $R1 -ne 0 ] || [ $R2 -ne 0 ]; then echo "RESULTADO: HAY CASOS FALLIDOS"; exit 1; fi
echo "RESULTADO: TODOS LOS CASOS APROBADOS"
