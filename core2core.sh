#!/usr/bin/env bash

OUT=corespp.csv
SIZE=128
COUNT=10000
TESTS="pipe_thr pipe_lat tcp_thr tcp_lat unix_thr unix_lat mempipe_thr memflag_lat"

ODIR=results
rm -rf ${ODIR}
mkdir -p ${ODIR}

for c1 in {0..47..1}; do
  for c2 in {0..47..1}; do
    echo ${c1} to ${c2}
    for t in ${TESTS}; do
      d="${ODIR}/${c1}-${c2}-${t}"
      mkdir -p ${d}
      if [ ${c1} -eq ${c2} ]; then
        count=1
      else
        count=${COUNT}
      fi
      ./${t} -t -s ${SIZE} -c ${count} -a ${c1} -b ${c2} -o ${d}
    done
  done
done

