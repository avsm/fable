#!/usr/bin/env bash

OUT=corespp.csv
SIZE=128
COUNT=200000
TESTS="pipe_thr pipe_lat tcp_thr tcp_lat unix_thr unix_lat"

echo core1 core2 name size count result > ${OUT}
for c1 in {0..47..1}; do
  for c2 in {0..47..1}; do
    echo ${c1} to ${c2}
    for t in ${TESTS}; do
      echo -n "${c1} ${c2} " >> ${OUT}
      ./${t} -s ${SIZE} -c ${COUNT} -a ${c1} -b ${c2} >> ${OUT}
    done
  done
done
