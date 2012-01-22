#!/bin/bash

CUR_DIR=$(pwd)

RESULTS_DIR=../../results
OUT_DIR=/usr/groups/netos/html/ipc-bench
HTML_OUTFILE=index.html

cd ${RESULTS_DIR}
RESULTS=$(ls tmp*.tar.gz)
cd ${CUR_DIR}

cat ${PWD}/html_head.tmpl > ${OUT_DIR}/${HTML_OUTFILE}
for r in ${RESULTS}; do
  ARCHIVE=${r}
  echo -n "Extracting ${ARCHIVE}..."
  tar -xzf ${RESULTS_DIR}/${ARCHIVE}
  echo " done!"
  DIR=tmp/$(echo ${r} | sed 's/\.tar\.gz//g')
  # Check the extracted data directory exists
  if [[ ! -d ${DIR} ]]; then
    echo "FATAL: After extraction, ${DIR} did not exist!"
  fi
  mkdir -p ${DIR}/graphs

  # Find the target cores used
  TARGET_CPUS=$(cat ${DIR}/logs/target_cpus)

  # Now extract system information and results
  python process.py ${DIR} ${OUT_DIR}/${HTML_OUTFILE} ${TARGET_CPUS}
done
cat ${PWD}/html_foot.tmpl >> ${OUT_DIR}/${HTML_OUTFILE}

rm -rf ${OUT_DIR}/tmp
mv tmp ${OUT_DIR}/
chmod -R g+rx ${OUT_DIR}/tmp
chmod -R o+rx ${OUT_DIR}/tmp
