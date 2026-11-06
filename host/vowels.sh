#!/bin/bash
# Formants of each vowel preset on this computer: host/vowels.sh [freq]
cd "$(dirname "$0")/.."
F=${1:-120}
cc -O2 -o host/render host/render.c -lm || exit 1
while read -r name idx dia lip target; do
  [ -z "$name" ] && continue
  ./host/render /tmp/pt_$name.raw 1.0 "$F" "$idx" "$dia" "$lip" 0.6 0.01
  printf '%-4s target %-10s ' "$name" "$target"; python3 host/formants.py /tmp/pt_$name.raw
done <<'V'
AH 12 2.3 3.0 730/1090
AE 22 3.4 3.0 660/1720
EH 24 2.7 3.0 530/1840
IH 26 2.4 3.0 390/1990
EE 27 2.1 3.0 270/2290
UH 12.9 2.43 3.0 520/1190
AW 15 2.05 1.3 570/840
OH 18 2.3 0.6 450/880
OO 14 2.05 0.5 300/870
V
