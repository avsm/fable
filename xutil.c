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
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include "atomicio.h"

void *
xmalloc(size_t size)
{
  void *buf;
  buf = malloc(size);
  if (buf == NULL)
    err(1, "xmalloc");
  return buf;
}

void
xread(int fd, void *buf, size_t count)
{
  ssize_t r;
  r = atomicio(read, fd, buf, count);
  if (r != count)
    err(1, "xread");
}

void
xwrite(int fd, void *buf, size_t count)
{
  ssize_t r;
  r = atomicio(vwrite, fd, buf, count);
  if (r != count)
    err(1, "xwrite");
}

#include <sched.h>
static void
setaffinity(int cpunum)
{
  cpu_set_t *mask;
  size_t size;
  int i;
  int nrcpus = 48;
  pid_t pid;
  mask = CPU_ALLOC(nrcpus);
  size = CPU_ALLOC_SIZE(nrcpus);
  CPU_ZERO_S(size, mask);
  CPU_SET_S(cpunum, size, mask);
  pid = getpid();
  i = sched_setaffinity(pid, size, mask);
  if (i == -1)
    err(1, "sched_setaffinity");
  CPU_FREE(mask);
}

/* Fork and pin to different CPUs */
int
xfork(void)
{
  char *affinity = getenv("SEPARATE_CPU");
  int cpu1 = 0;
  int cpu2 = 0;
  if (affinity != NULL) cpu2 = 1;
  if (!fork()) { /* child */
    setaffinity(cpu1);
    return 0;
  } else { /* parent */ 
    setaffinity(cpu2);
    return 1;
 }
}
