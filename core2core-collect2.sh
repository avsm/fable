#!/usr/bin/env bash

OUT=results-mempipe
COLLECT=collected-mempipe

rm -rf ${COLLECT}
mkdir -p ${COLLECT}
t="mempipe_thr"
for c1 in `jot 48 0`; do
  for c2 in `jot 48 0`; do
  d="${OUT}/${c1}-${c2}/01-${t}-headline.log"
  speed=`tail -1 ${d} | awk '{print $4}' | sed -e 's/s//g' | sed -e 's/Mbp$//g'`
  echo -n "${speed} " >> ${COLLECT}/${t}.csv
done
echo "" >> ${COLLECT}/${t}.csv
done
