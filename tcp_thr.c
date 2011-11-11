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

static void
init_test(test_data *td)
{
  int ret;
  char portbuf[32];
  struct addrinfo hints;
  int port = 3490+td->num;
  snprintf(portbuf, sizeof portbuf, "%d", port);
  fprintf(stderr, "pid %d init_test port %d\n", getpid(), port);
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  if ((ret = getaddrinfo("127.0.0.1", portbuf, &hints, (struct addrinfo **)&td->data)) != 0)
    errx(1, "getaddrinfo: %s\n", gai_strerror(ret));
}

static void
run_child(test_data *td)
{
  int sockfd, new_fd, i;
  struct addrinfo *res = (struct addrinfo *)td->data;
  struct sockaddr_storage their_addr;
  socklen_t addr_size;
  void *buf = xmalloc(td->size);
 
  fprintf(stderr, "run_child num %d size %d count % " PRId64 "\n", td->num, td->size, td->count);
  if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1)
    err(1, "socket");

  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(int)) == -1)
    err(1, "setsockopt");
    
  while (bind(sockfd, res->ai_addr, res->ai_addrlen) == -1)
    { }
    
  if (listen(sockfd, 1) == -1)
    err(1, "listen");
    
  addr_size = sizeof their_addr;
    
  if ((new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size)) == -1)
    err(1, "accept");

  for (i = 0; i < td->count; i++) 
    xread(new_fd, buf, td->size);
  xwrite(new_fd, "X", 1);
}

static void
run_parent(test_data *td)
{
  struct addrinfo *res = (struct addrinfo *)td->data;
  int sockfd;
  void *buf = xmalloc(td->size);
  sleep(1); 
  if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1)
    err(1, "socket");
    
  if (connect(sockfd, res->ai_addr, res->ai_addrlen) == -1)
    err(1, "connect");

  thr_test(
    do {
      xwrite(sockfd, buf, td->size);
    } while (0),
    do {
      xread(sockfd, buf, 1);
    } while (0),
    td
  );
}

int
main(int argc, char *argv[])
{
  test_t t = { "tcp_thr", init_test, run_parent, run_child };
  run_test(argc, argv, &t);
  return 0;
}
