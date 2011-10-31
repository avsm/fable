#!/usr/bin/env bash

SIZES="1 2 8 16 32 64 128 256 512 1024 2048 4096 8192 16384 32768 65536"
NUMS="50000"
TEST="pipe_lat unix_lat"

for test in ${TEST}; do
  for size in ${SIZES}; do
    for num in ${NUMS}; do
      echo ${test}: ${size} x ${num}
      ./${test} ${size} ${num}
    done
  done
done
