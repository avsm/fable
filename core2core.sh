#!/usr/bin/env bash

OUT=corespp.csv
SIZE=128
COUNT=20000
TESTS="pipe_thr pipe_lat tcp_thr tcp_lat unix_thr unix_lat mempipe_thr"

ODIR=results
rm -rf ${ODIR}
mkdir -p ${ODIR}

for c1 in {0..47..1}; do
  for c2 in {0..47..1}; do
    echo ${c1} to ${c2}
    for t in ${TESTS}; do
      d="${ODIR}/${c1}-${c2}-${t}"
      mkdir -p ${d}
      ./${t} -s ${SIZE} -c ${COUNT} -a ${c1} -b ${c2} -o ${d}
    done
  done
done

ODIR=results-mempipe
rm -rf ${ODIR}
mkdir -p ${ODIR}
SIZE=4096
COUNT=100000
for c1 in {0..47..1}; do
  for c2 in {0..47..1}; do
    echo ${c1} to ${c2}
    d="${ODIR}/${c1}-${c2}"
    mkdir -p ${d}
    ./mempipe_thr -s ${SIZE} -c ${COUNT} -a ${c1} -b ${c2} -o ${d}
  done
done
