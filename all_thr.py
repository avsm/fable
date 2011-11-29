#!/usr/bin/python

# This script runs the following battery of tests:
# Chunk sizes: 64, 4096, 65536
# Test types: pipe, unix, tcp, tcp-nodelay, vmsplice-coop, shmem-pipe, mempipe-futex, mempipe-spin
# The latter 3 tests are repeated with a "safe" form that has the writer copy the data from shared memory before verification
# All tests use "sos22_memset" (rep stosq) to write buffers (data is the iteration count) and rep scasq to verify at the reader.
# All tests work in place so far as their underlying transport allows (so pipes involve 2 copies, vmsplice involves 1, rings involve 0).
# All tests will be repeated to test bandwidth between cores named on the command line: see below...

# Parameters for this script: 1st parameter is an output directory (will be created; all tests will be stored below this)
# All following parameters must be integers denoting a core. If you supply [x,y,z] we will test 0-0, 0-x, 0-y, 0-z.
# We omit the mempipe-spin 0-0 test as it's stupid.

import sys
import errno
import subprocess
import os

output_dir = sys.argv[1]
try:
    os.makedirs(output_dir)
except OSError as e:
    if e.errno == errno.EEXIST:
        pass
    else:
        raise

target_cores = ["0"]
target_cores.extend([str(int(x)) for x in sys.argv[2:]])

chunk_repeats = [("64", "1000000"), ("4096", "1000000"), ("65536", "100000")]

test_rips = [("pipe_thr", True), ("unix_thr", True), 
             ("tcp_thr", True), ("tcp_nodelay_thr", True),
             ("vmsplice_coop_pipe_thr", True), 
             ("mempipe_thr", True), ("mempipe_thr", False),
             ("mempipe_spin_thr", True), ("mempipe_spin_thr", False), 
             ("shmem_pipe_thr", True), ("shmem_pipe_thr", False)]

for test, rip in test_rips:
    for chunksize, repeats in chunk_repeats:
        for tcore in target_cores:
            
            if test == "mempipe_spin_thr" and tcore == "0":
                continue

            progname = "./%s" % test
            args = [progname, "-t", "-s", chunksize, "-c", repeats, "-a", "0", "-b", tcore, "-o", output_dir, "-w", "-v", "-m", "2"]
            # Last four args: write in place, do verify, produce using rep stosq
            if rip:
                args.append("-r")
            sys.stdout.write("%s: " % test)
            sys.stdout.flush()
            subprocess.check_call(args)

