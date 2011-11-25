/* 
    Measure latency of IPC using tcp sockets

    Copyright (c) 2011 Steven Smith <sos22@cam.ac.uk>

    Permission is hereby granted, free of charge, to any person
    obtaining a copy of this software and associated documentation
    files (the "Software"), to deal in the Software without
    restriction, including without limitation the rights to use,
    copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following
    conditions:

    The above copyright notice and this permission notice shall be
    included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
    OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
    HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
    WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
    OTHER DEALINGS IN THE SOFTWARE.
*/

/* Ultimate in simple IPC protocols: establish a shared memory
   segment and then send a ping by setting flags in it, with the other
   end spinning reading those flags.
   Note that this is a straight up latency test: no actual data is
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
child_ping(test_data *td)
{
  volatile struct shared_page *sp = td->data;
  sp->flag1 = 1;
  while (!sp->flag2)
    ;
  sp->flag1 = 0;
  while (sp->flag2)
    ;
}

static void child_finish(test_data *td) {
  volatile struct shared_page *sp = td->data;
  sp->flag1 = 1;
}

static void
parent_init(test_data *td)
{
  volatile struct shared_page *sp = td->data;

  /* Wait for the child to get ready before starting the test. */
  while (!sp->flag1)
    ;
}

static void parent_ping(test_data* td) {

  volatile struct shared_page *sp = td->data;

  sp->flag2 = 1;
  while (sp->flag1)
    ;
  sp->flag2 = 0;
  while (!sp->flag1)
    ;

}

int
main(int argc, char *argv[])
{
  test_t t = { 
    .name = "mempipe_lat",
    .is_latency_test = 1,
    .init_test = init_test, 
    .init_parent = parent_init,
    .parent_ping = parent_ping,
    .child_ping = child_ping,
    .finish_child = child_finish
  };
  run_test(argc, argv, &t);
  return 0;
}
