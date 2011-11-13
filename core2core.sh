#!/usr/bin/env bash

SIZES="128 4096 65536"
COUNT=10000
THR_TESTS="pipe_thr tcp_thr unix_thr mempipe_thr tcp_nodelay_thr vmsplice_pipe_thr"
LAT_TESTS="pipe_lat tcp_lat unix_lat mempipe_lat tcp_nodelay_lat"
TESTS="${THR_TESTS} ${LAT_TESTS}"

ODIR=results
rm -rf ${ODIR}
mkdir -p ${ODIR}

for SIZE in ${SIZES}; do
for c1 in {0..47..1}; do
  for c2 in {0..47..1}; do
    echo ${c1} to ${c2}
    for t in ${TESTS}; do
      d="${ODIR}/${SIZE}/${c1}-${c2}-${t}"
      mkdir -p ${d}
      if [ ${c1} -eq ${c2} ]; then
        count=100
      else
        count=${COUNT}
      fi
      ./${t} -t -s ${SIZE} -c ${count} -a ${c1} -b ${c2} -o ${d}
    done
  done
done
done
