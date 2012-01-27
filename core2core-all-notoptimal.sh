#!/usr/bin/env bash

SIZE="4096"
COUNT=20000000
TESTS="pipe_thr"

ODIR="results.allthr.native.notopt"
rm -rf ${ODIR}
mkdir -p ${ODIR}

for base in {0..7..1}; do
  echo -n "${base}:  "
  for c in {0..2..1}; do
    c1=$(((${base} * 6) + (${c} * 2)))
    c2=$((47-${c1}))
    echo -n "${c1}x${c2} "
    for t in ${TESTS}; do
      d="${ODIR}/${SIZE}/${c1}-${c2}-${t}"
      mkdir -p ${d}
      ./${t} -s ${SIZE} -c ${COUNT} -a ${c1} -b ${c2} -o ${d} -m 2 &
    done
  done
  echo
done

wait
