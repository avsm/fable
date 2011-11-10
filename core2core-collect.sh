#!/usr/bin/env bash
set -e
NAMES="native48 xen48nopin xen48pin"
COLLECT=graph
TESTS="pipe_thr pipe_lat tcp_thr tcp_lat unix_thr unix_lat mempipe_thr"
NCORES=48

for NAME in ${NAMES}; do
OUT=results.${NAME}
idx="${COLLECT}/${NAME}_all.js"
echo "if (!tests) tests={};" > ${idx}
for t in ${TESTS}; do
  name="${NAME}_${t}"
  ofile=${COLLECT}/${name}.js
  rm -f ${ofile}
  echo "var ${name} = [" >> ${ofile}
  for c1 in `jot ${NCORES} 0`; do
    echo " [" >> ${ofile}
    for c2 in `jot ${NCORES} 0`; do
      d="${OUT}/${c1}-${c2}-${t}/01-${t}-headline.log"
      speed=`tail -1 ${d} | awk '{print $4}' | sed -e 's/s//g' | sed -e 's/Mbp$//g'`
      echo -n "${speed}," >> ${ofile}
    done
    echo "]," >> ${ofile}
  done
  echo "];" >> ${ofile}
  cat ${ofile} >> ${idx}
  echo "tests[\"${name}\"] = ${name};" >> ${idx}
done
done
