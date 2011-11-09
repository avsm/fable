#!/usr/bin/env bash

OUT=results
COLLECT=collected
TESTS="pipe_thr pipe_lat tcp_thr tcp_lat unix_thr unix_lat mempipe_thr"

rm -rf ${COLLECT}
mkdir -p ${COLLECT}
for t in ${TESTS}; do
  for c1 in `jot 48 0`; do
    for c2 in `jot 48 0`; do
      d="${OUT}/${c1}-${c2}-${t}/01-${t}-headline.log"
      speed=`tail -1 ${d} | awk '{print $4}' | sed -e 's/s//g' | sed -e 's/Mbp$//g'`
      echo -n "${speed} " >> ${COLLECT}/${t}.csv
    done
    echo "" >> ${COLLECT}/${t}.csv
  done
done
