#!/bin/bash
# view2.sh region page(0/1) "eemods (vuoto = default del gioco)" out.png
reg=$1; page=$2
pre="T W150 D D D D D D 1 W60"
if [ "$page" = 1 ]; then pre="$pre D 1 W60"; else pre="$pre 1 W60"; fi
( [ -n "$3" ] && python3 $(dirname $0)/eeimg_r.py $reg "$3"; HOLD=1 GAP=30 python3 $(dirname $0)/seq.py 200 "$pre P" 2>/dev/null) > /tmp/v.txt
end=$(grep shot /tmp/v.txt | tail -1 | awk '{print $2}')
rm -f /tmp/vq_*
FAST=1 ${SNKHD_CPS3:-build/snkhd_cps3} ${REDEARTH_ZIP:-redearth.zip} redearth $reg $((end+2)) 1 1 /tmp/vq /tmp/v.txt >/dev/null 2>&1
cp /tmp/vq_${end}.png $4
