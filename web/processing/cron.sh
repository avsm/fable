#!/bin/bash

CUR_DIR=$(pwd)

RESULTS_DIR=../../results
OUT_DIR=../public
HTML_OUTFILE=index.html

cd ${RESULTS_DIR}
RESULTS=$(ls tmp*.tar.gz)
cd ${CUR_DIR}

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
  cat ${PWD}/html_head.tmpl > ${OUT_DIR}/${HTML_OUTFILE}
  python process.py ${DIR} ${OUT_DIR}/${HTML_OUTFILE} ${TARGET_CPUS}
  cat ${PWD}/html_foot.tmpl >> ${OUT_DIR}/${HTML_OUTFILE}
done

rm -rf ${OUT_DIR}/tmp
mv tmp ${OUT_DIR}/
