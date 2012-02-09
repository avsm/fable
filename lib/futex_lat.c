/* Simple futex latency tester.  Basic protocol looks like this:

   1) Create a shared memory area.
   2) Fork off a child.
   Parent:                          Child
      3a) Pinself to CPU 0          3b) Pin self to some other CPU
      4a) Waits 10 milliseconds     4b) Futex wait while shared memory
                                        area == 0
      5a) Starts the main timer
      6a) Sets shared memory area
          to 1.
      7a) Issues futex wake
      8a) Spins waiting for shared  5b) Wakes up
	  memory area to be 2.	    6b) Sets shared memory area to 2
      9a) Stops timer               7b) Exits

   And we then repeat that a bunch of times.

   Note that this isn't the same protocol as we use for most other
   latency tests, which are mostly ping-pong based, because it's hard
   to design a ping-pong futex test which ensures that the waking
   process is actually blocked in the futex call when you issue the
   futex wake, rather than being short-circuited through userspace.
   This means that, if you're not very careful, you end up measuring
   the cost of atomic operations rather than futex operations.  Doing
   single-shot operations avoids this problem, because we can just do
   a usleep() while the child gets into the right state.

   The downside of this approach is that the number you get out isn't
   entirely meaningful, because the harness gets in the way.  However,
   the difference between the futex latency and the spinning latency
   *is* meaningful, because they're using the same harness, and we
   have an independent way of estimating the spinning latency
   (mempipe_lat), so you can actually do something useful with the
   result. */
#include <sys/types.h>
#include <sys/time.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <linux/futex.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "xutil.h"
#include "futex.h"
#include "test.h"

#define USE_FUTEXES

static void
run_child(void *shm)
{
  cpu_set_t ss;
  CPU_ZERO(&ss);
  CPU_SET(2, &ss);
  if (sched_setaffinity(0, sizeof(ss), &ss) < 0)
    err(1, "set_affinity");
  while (*(volatile unsigned *)shm == 0)
#ifdef USE_FUTEXES
    futex_wait_while_equal(shm, 0)
#endif
      ;
  *(unsigned *)shm = 2;
  exit(0);
}

static unsigned long
run_parent(void *shm)
{
  unsigned long start_tsc;
  unsigned long end_tsc;
  cpu_set_t ss;

  CPU_ZERO(&ss);
  CPU_SET(0, &ss);
  if (sched_setaffinity(0, sizeof(ss), &ss) < 0)
    err(1, "set_affinity");

  usleep(10000);

  start_tsc = rdtsc();
  *(unsigned *)shm = 1;
#ifdef USE_FUTEXES
  futex_wake(shm);
#endif
  while (*(volatile unsigned *)shm == 1)
    ;
  end_tsc = rdtsc();

  return end_tsc - start_tsc;
}

static unsigned long
doit(void *shm)
{
  pid_t child;
  unsigned long res;
  int status;

  child = fork();
  if (child == 0) {
    run_child(shm);
    _exit(0);
  } else {
    res = run_parent(shm);
    if (waitpid(child, &status, 0) != child)
      err(1, "waitpid");
    if (WIFSIGNALED(status))
      errx(1, "child died with signal %d", WTERMSIG(status));
    if (!WIFEXITED(status))
      errx(1, "child abnromal exit code 0x%x", status);
    if (WEXITSTATUS(status) != 0)
      errx(1, "child reported error %d", WEXITSTATUS(status));
    *(unsigned *)shm = 0;
    return res;
  }
}

int
main(int argc, char *argv[])
{
  void *shm = establish_shm_segment(1, -1);
  int i;

  for (i = 0; i < 100; i++)
    printf("%ld\n", doit(shm));
  return 0;
}
