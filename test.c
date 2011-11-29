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

#include <sys/stat.h>
#include <assert.h>
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

static void
wait_for_children_to_finish(void)
{
  int status, rv;

  /* Make sure the child really does go away when it's no longer
     needed. */
  while (1) {
    rv = waitpid(-1, &status, 0);
    assert(rv != 0);
    if (rv < 0) {
      if (errno == ECHILD)
	break;
      err(1, "waitpid()");
    }
    if (WIFSIGNALED(status))
      errx(1, "child killed by signal %d", WTERMSIG(status));
    if (WIFEXITED(status)) {
      if (WEXITSTATUS(status) != 0)
	errx(1, "child exited with status %d", WEXITSTATUS(status));
    } else {
      errx(1, "unexpected status %x from waitpid", status);
    }
  }
}

/* Execute a test with as many parallel iterations as requested */
void
run_test(int argc, char *argv[], test_t *test)
{ 
  bool per_iter_timings;
  int first_cpu, second_cpu;
  size_t count;
  int size, parallel;
  char *output_dir;
  int mode;
  int numa_node;

  parse_args(argc, argv, &per_iter_timings, &size, &count, &first_cpu, &second_cpu, &parallel, &output_dir, &mode,
	     &numa_node);

  if (mkdir(output_dir, 0755) < 0 && errno != EEXIST)
    err(1, "creating directory %s", output_dir);

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
      td->mode = mode;
      td->numa_node = numa_node;
      /* Test-specific init */
      test->init_test(td); 
      pid_t pid2 = fork ();
      if (!pid2) { /* child2 */
        setaffinity(first_cpu);
        test->run_child(td);
        exit (0);
      } else { /* parent2 */
	td->output_dir = output_dir; /* Do this here because the child
					isn't supposed to log
					anything. */
	td->name = test->name;
        setaffinity(second_cpu);
        test->run_parent(td);

	wait_for_children_to_finish();

        exit (0);
      }
    } else { /* parent */ 
      /* continue to fork */
      parallel--;
    }
  }
  wait_for_children_to_finish();
}
