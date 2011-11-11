/* 
    Copyright (c) 2011 Anil Madhavapeddy <anil@recoil.org>
    Copyright (c) 2011 Chris Smowton <chris@smowton.net>

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
#include <fcntl.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <errno.h>

#include "test.h"
#include "xutil.h"

typedef struct {
  int fds[2];
} pipe_state;

static void
init_test(test_data *td)
{
  pipe_state *ps = xmalloc(sizeof(pipe_state));
  if (pipe(ps->fds) == -1)
    err(1, "pipe");
  td->data = (void *)ps;
}

static void
run_child(test_data *td)
{
  pipe_state *ps = (pipe_state *)td->data;
  void *buf = xmalloc(td->size);
  int i;

  for (i = 0; i < td->count; i++) {
    xread(ps->fds[0], buf, td->size);
  }
}

static void
run_parent(test_data *td)
{
  pipe_state *ps = (pipe_state *)td->data;
  void *buf = xmalloc(td->size);
  struct iovec iov;

  thr_test(
    do {
      iov.iov_base = buf;
      iov.iov_len = td->size;
      while(iov.iov_len) {
	ssize_t this_write = vmsplice(ps->fds[1], &iov, 1, 0);
	if(this_write < 0)
	  err(1, "vmsplice");
	else if(this_write == 0)
	  break;
	iov.iov_len -= this_write;
	iov.iov_base = ((char*)iov.iov_base) + this_write;
      }
    } while (0),
    do {} while (0),
    td
  );
}

int
main(int argc, char *argv[])
{
  test_t t = { "vmsplice_pipe_thr", init_test, run_parent, run_child };
  run_test(argc, argv, &t);
  return 0;
}
