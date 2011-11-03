/*
 * Copyright (c) 2011 Anil Madhavapeddy <anil@recoil.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/time.h>
#include <err.h>
#include <inttypes.h>
#include <netdb.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#include "test.h"
#include "xutil.h"

/* Execute a test with as many parallel iterations as requested */
void
run_test(int argc, char *argv[], test_t *test)
{ 
  bool per_iter_timings, separate_cpu;
  size_t count;
  int size, parallel;
  pid_t pid;
  char *output_dir;
  parse_args(argc, argv, &per_iter_timings, &size, &count, &separate_cpu, &parallel, &output_dir);
  while (parallel > 0) {
    pid_t pid1 = fork ();
    if (!pid1) { /* child1 */
      /* Initialise a test run */
      test_data *td = xmalloc(sizeof(test_data));
      memset(td, 0, sizeof(test_data));
      td->num = parallel;
      td->size = size;
      td->count = count;
      td->per_iter_timings = per_iter_timings;
      /* Test-specific init */
      test->init_test(td); 
      pid_t pid2 = fork ();
      if (!pid2) { /* child2 */
	/* Discourage child from writing to log file by not setting
	   td->logfile. */
        setaffinity(0);
        test->run_child(td);
        exit (0);
      } else { /* parent2 */
        int a = separate_cpu ? 1 : 0;
	char *logpath;
	if (asprintf(&logpath, "%s/%d.log", output_dir, parallel) < 0)
	  err(1, "asprintf()");
	td->logfile = fopen(logpath, "w");
	if (!td->logfile)
	  err(1, "cannot open %s", logpath);
	free(logpath);

        setaffinity(a);
        test->run_parent(td);

	fclose(td->logfile);

        exit (0);
      }
    } else { /* parent */ 
      /* continue to fork */
      parallel--;
    }
  }
  /* Wait for all the spawned tests to finish */
  while ((pid = waitpid(-1, NULL, 0))) {
    if (errno == ECHILD) {
      break;
    }
  }
}
