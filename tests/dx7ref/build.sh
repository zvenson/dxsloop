#!/bin/sh
# builds tests/dx7_exact_test against a Dexed msfa checkout (MSFA=dir with dx7note.cc etc., JUCE-free)
set -e
cd "$(dirname "$0")/../.."
MSFA="${MSFA:-$HOME/fm1-drums/engine/msfa}"
OUT=build/host/dx7ref; mkdir -p "$OUT"
cc -O2 -c -Ibuild/gen -Ifirmware/src tests/dx7ref/dx7_c_wrap.c -o "$OUT/cport.o"
for f in dx7note env exp2 fm_core fm_op_kernel freqlut lfo pitchenv sin tuning; do
  c++ -std=c++17 -O2 -w -I"$MSFA" -I"$MSFA/.." -c "$MSFA/$f.cc" -o "$OUT/$f.o"; done
c++ -std=c++17 -O2 -w -I"$MSFA" -c "$MSFA/porta.cpp" -o "$OUT/porta.o"
c++ -std=c++17 -O2 -w -I"$MSFA" -c "$MSFA/libMTSClient.cpp" -o "$OUT/mts.o"
c++ -std=c++17 -O2 -w -I"$MSFA" tests/dx7_exact_test.cc "$OUT"/*.o -ldl -o "$OUT/dx7_exact_test"
echo "built $OUT/dx7_exact_test"
