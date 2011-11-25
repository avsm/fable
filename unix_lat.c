/* 
    Copyright (c) 2011 Anil Madhavapeddy <anil@recoil.org>

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

#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#include "test.h"
#include "xutil.h"

typedef struct {
  int sv[2];
  void* buf;
} test_state;

static void
init_test(test_data *td)
{
  test_state *ts = xmalloc(sizeof(test_state));
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, ts->sv) == -1)
    err(1, "socketpair");
  td->data = (void *)ts;
}

static void
local_init(test_data *td)
{
  test_state *ps = (test_state *)td->data;
  ps->buf = xmalloc(td->size);
}

static void
child_ping(test_data *td)
{
  test_state *ps = (test_state *)td->data;
  xread(ps->sv[1], ps->buf, td->size);
  xwrite(ps->sv[1], ps->buf, td->size);
}

static void
parent_ping(test_data *td)
{
  test_state *ps = (test_state *)td->data;
  xwrite(ps->sv[0], ps->buf, td->size);
  xread(ps->sv[0], ps->buf, td->size);
}

int
main(int argc, char *argv[])
{
  test_t t = { .name = "unix_lat", 
	       .is_latency_test = 1,
	       .init_test = init_test,
	       .init_parent = local_init,
	       .init_child = local_init,
	       .parent_ping = parent_ping,
	       .child_ping = child_ping
  };
  run_test(argc, argv, &t);
  return 0;
}
