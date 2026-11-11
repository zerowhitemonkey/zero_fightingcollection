#!/bin/bash
# view.sh page(0 sys/1 game) "eemods" out.png
page=$1
pre="T W150 D D D D D D 1 W60"
if [ "$page" = 1 ]; then pre="$pre D 1 W60"; else pre="$pre 1 W60"; fi
(python3 $(dirname $0)/eeimg.py "$2"; HOLD=1 GAP=30 python3 $(dirname $0)/seq.py 200 "$pre P" 2>/dev/null) > /tmp/v.txt
end=$(grep shot /tmp/v.txt | tail -1 | awk '{print $2}')
rm -f /tmp/vq_*
FAST=1 ${SNKHD_CPS3:-build/snkhd_cps3} ${REDEARTH_ZIP:-redearth.zip} redearth 53 $((end+2)) 1 1 /tmp/vq /tmp/v.txt >/dev/null 2>&1
cp /tmp/vq_${end}.png $3
