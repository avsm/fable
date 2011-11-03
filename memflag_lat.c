/* Ulitimate in simple IPC protocols: establish a shared memory
   segment and then send a ping by setting flags in it, with the other
   end spinning reading those flags. */
/* Note that this is a straight up latency test: no actual data is
   transferred. */
#include <sys/stat.h>
#include <sys/time.h>
#include <err.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#include "test.h"
#include "xutil.h"

#define PAGE_SIZE 4096

struct shared_page {
  int flag1;
  int pad[127]; /* Make sure the two flags are in different cache lines
		   for any conceivable size fo cache line. */
  int flag2;
};

static void
init_test(test_data *td)
{
  td->data = establish_shm_segment(1);
}

static void
run_child(test_data *td)
{
  volatile struct shared_page *sp = td->data;
  int i;

  for (i = 0; i <= td->count; i++) {
    sp->flag1 = 1;
    while (!sp->flag2)
      ;
    sp->flag1 = 0;
    while (sp->flag2)
      ;
  }
  sp->flag1 = 1;
}

static void
run_parent(test_data *td)
{
  volatile struct shared_page *sp = td->data;

  /* Wait for the child to get ready before starting the test. */
  while (!sp->flag1)
    ;

  latency_test(
    "memflag_lat",
    do {
      sp->flag2 = 1;
      while (sp->flag1)
	;
      sp->flag2 = 0;
      while (!sp->flag1)
	;
    } while (0),
    td
  );
}

int
main(int argc, char *argv[])
{
  test_t t = { init_test, run_parent, run_child };
  run_test(argc, argv, &t);
  return 0;
}
