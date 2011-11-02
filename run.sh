#!/usr/bin/env bash
set -e

#SIZES="1 2 8 16 32 64 128 256 512 1024 2048 4096 8192 16384 32768 65536"
#SIZES="1 8 32 64 256 1024 4096 8192 12288 16384 20480 24576 28672 32768 65536"
SIZES="1 64 256 1024 4096 8192 12288 16384 20480 24576 28672 32768 36864 40960 45056 49152 53248 57344 61440 65536 73728 79740 100000 105406 110000 131072"

NUMS="100000"
TEST="pipe_lat unix_lat tcp_lat tcp_thr pipe_thr unix_thr"

IF=eth0
EXTERNAL_IP=$(/sbin/ifconfig eth0 | grep 'inet addr:' | cut -d: -f2 | awk '{ print $1}')

OUT=results-same.csv
echo name size count result > ${OUT}

for test in ${TEST}; do
  for size in ${SIZES}; do
    for num in ${NUMS}; do
      echo ${test}: ${size} x ${num}
      ./${test} ${size} ${num} >> ${OUT}
    done
  done
done

export SEPARATE_CPU=yes
OUT=results-diff.csv
echo name size count result > ${OUT}

for test in ${TEST}; do
  for size in ${SIZES}; do
    for num in ${NUMS}; do
      echo ${test}: ${size} x ${num}
      ./${test} ${size} ${num} >> ${OUT}
    done
  done
done
