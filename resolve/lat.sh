#!/usr/bin/env bash

ofile=${ARCH}.${TEST}.lat.csv
rm -f ${ofile}
for c1 in `jot 48 0`; do
  for c2 in `jot 48 0`; do
    d="results.${ARCH}/${SIZE}/${c1}-${c2}-${TEST}/01-${TEST}-headline.log"
    speed=`tail -1 ${d} | awk '{print $4}' | sed -e 's/s//g'`
    echo -n "${speed} " >> ${ofile}
  done
  echo "" >> ${ofile}
done
