/* Ulitimate in simple IPC protocols: establish a shared memory
   segment and then send a ping by setting flags in it, with the other
   end spinning reading those flags. */
/* Note that this is a straight up latency test: no actual data is
   transferred. */
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <err.h>
#include <fcntl.h>
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
  int fd;
  void *addr;

  fd = shm_open("/memflag_lat", O_RDWR|O_CREAT|O_EXCL, 0600);
  if (fd < 0)
    err(1, "shm_open(\"/memflag_lat\")");
  shm_unlink("/memflag_lat");
  if (ftruncate(fd, PAGE_SIZE) < 0)
    err(1, "ftruncate() shared memory segment");
  addr = mmap(NULL, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED,
	      fd, 0);
  if (addr == MAP_FAILED)
    err(1, "mapping shared memory segment");
  close(fd);

  td->data = addr;

  printf("Mapped to %p\n", td->data);
}

static void
run_child(test_data *td)
{
  volatile struct shared_page *sp = td->data;
  int i;

  for (i = 0; i < td->count; i++) {
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

  printf("Done test\n");
}

int
main(int argc, char *argv[])
{
  test_t t = { init_test, run_child, run_parent };
  run_test(argc, argv, &t);
  return 0;
}
