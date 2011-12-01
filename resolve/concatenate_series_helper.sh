#!/bin/bash


for m in mempipe shmem_pipe vmsplice_coop_pipe pipe unix tcp tcp_nodelay; do
  testfile=01-${m}_thr-headline.log
  for i in 2 3 4 5 6 7 8 9 10; do
    if [[ $i == 2 ]]; then
      in1=1/$testfile
    else
      in1=tmp$j
    fi
    in2=$i/$testfile
    out=tmp$i
    paste -d ' ' $in1 <(cut -d ' ' -f 11 $in2) > $out
    j=$i
  done
  mv tmp10 $testfile
  rm tmp*
done
