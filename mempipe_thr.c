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

/* Simple shared-memory IPC throughput test, with spin-waits rather
   than blocking.  The shared area is divided into packets, each of
   which starts with a header.  The header is the size of a cache
   line, but the only really interesting field is the size of the
   packet.  To transmit, you just copy the data you want to transmit
   it and then go back and write the header.  Meanwhile, the receiver
   will be sitting spinning waiting for the header to arrive, and will
   read the packet out as soon as it does.  Once it's done so, it'll
   go back and set the finished flag in the header, so that the
   transmitter knows when it's safe to reuse a bit of buffer for a new
   message.  The benchmark which we build on top of this just keeps
   sending fixed-sized messages for soem number of times, waiting as
   appropriate for the receiver to pick them up. */

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <linux/futex.h>
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#undef USE_MWAIT
#define USE_FUTEX

#include "test.h"
#include "xutil.h"
#include "futex.h"

#define PAGE_SIZE 4096
#define CACHE_LINE_SIZE 64
static unsigned nr_shared_pages = 512;
#define ring_size (PAGE_SIZE * nr_shared_pages)

#ifdef SOS22_MEMSET
#define real_memset mymemset
#define test_name "mempipe_thr_sos22"

static void mymemset(void* buf, int byte, size_t count) {
  int clobber;
  assert(count % 8 == 0);
  asm volatile ("rep stosq\n"
		: "=c" (clobber)
		: "a" ((unsigned long)(byte & 0xff) * 0x0101010101010101ul),
		  "D" (buf),
		  "0" (count / 8)
		: "memory");
}

#else
#define real_memset memset
#define test_name "mempipe_thr"
#endif

struct msg_header {
#define MH_FLAG_READY 1
#define MH_FLAG_STOP 2
#define MH_FLAG_WAITING 4
#define MH_FLAGS (MH_FLAG_READY|MH_FLAG_STOP|MH_FLAG_WAITING)
  unsigned size_and_flags;
  int pad[CACHE_LINE_SIZE / sizeof(int) - 1];
};

static unsigned long
mask_ring_index(unsigned long idx)
{
  return idx & (ring_size - 1);
}

static void
init_test(test_data *td)
{
  td->data = establish_shm_segment(nr_shared_pages);
}

static void
consume_message(const void *data, unsigned long offset, unsigned size,
		void *outbuf)
{
  offset = mask_ring_index(offset);
  if (offset + size <= ring_size) {
    memcpy(outbuf, data + offset, size);
  } else {
    memcpy(outbuf, data + offset, ring_size - offset);
    memcpy(outbuf + ring_size - offset, data, size - (ring_size - offset));
  }
}

static void
copy_message(void *data, unsigned long offset, unsigned size, const void *inbuf)
{
  offset = mask_ring_index(offset);
  if (offset + size <= ring_size) {
    memcpy(data + offset, inbuf, size);
  } else {
    memcpy(data + offset, inbuf, ring_size - offset);
    memcpy(data, inbuf + (ring_size - offset), size - (ring_size - offset));
  }
}

static void
set_message(void *data, unsigned long offset, unsigned size, int byte) 
{
  offset = mask_ring_index(offset);
  if (offset + size <= ring_size) {
    real_memset(data + offset, byte, size);
  } else {
    real_memset(data + offset, byte, ring_size - offset);
    real_memset(data, byte, size - (ring_size - offset));
  }
}

#ifdef USE_MWAIT
static void
mwait(void)
{
  asm volatile("mwait\n"
	       :
	       : "a" (0), "c" (0));
}
static void
monitor(const volatile void *what)
{
  asm volatile("monitor\n"
	       :
	       : "a" (what),
		 "c" (0),
		 "d" (0)
	       );
}
#else
static void
mwait(void)
{
}
static void
monitor(const volatile void *what)
{
}
#endif

/* Deferred write state */
#ifdef USE_FUTEX
static struct {
  volatile unsigned *ptr;
  unsigned val;
  unsigned cntr;
} deferred_write;

static void
_set_message_ready(volatile unsigned *ptr, unsigned val)
{
  int sz;
  sz = atomic_xchg(ptr, val);
  if (sz & MH_FLAG_WAITING)
    futex_wake(ptr);
}
#endif

static int
wait_for_message_ready(volatile struct msg_header *mh, int desired_state)
{
  int sz;
#ifdef USE_FUTEX
  int new_sz;
  if (deferred_write.ptr) {
    _set_message_ready(deferred_write.ptr, deferred_write.val);
    deferred_write.ptr = NULL;
  }
  while (1) {
    sz = mh->size_and_flags;
    if ((sz & MH_FLAG_READY) == desired_state)
      break;
    new_sz = sz | MH_FLAG_WAITING;
    if (new_sz == sz ||
	atomic_cmpxchg(&mh->size_and_flags, sz, new_sz) == sz)
      futex_wait_while_equal(&mh->size_and_flags, new_sz);
  }
#else
  sz = mh->size_and_flags;
  if ((sz & MH_FLAG_READY) != desired_state) {
    while (1) {
      monitor(&mh->size_and_flags);
      sz = mh->size_and_flags;
      if ((sz & MH_FLAG_READY) == desired_state)
	break;
      mwait();
    }
  }
#endif
  return sz;
}

static void
set_message_ready(volatile struct msg_header *mh, int size)
{
#ifdef USE_FUTEX
  if (deferred_write.ptr) {
    deferred_write.cntr--;
    mh->size_and_flags = size;
    if (!deferred_write.cntr) {
      _set_message_ready(deferred_write.ptr, deferred_write.val);
      deferred_write.ptr = NULL;
    }
  } else {
    deferred_write.ptr = &mh->size_and_flags;
    deferred_write.val = size;
    deferred_write.cntr = 2;
  }
#else
  mh->size_and_flags = size;
#endif
}

static void
run_child(test_data *td)
{
  unsigned long next_message_start;
  volatile struct msg_header *mh = td->data;
  int sz;
  char *buf = xmalloc(td->size);

  /* Sync up with parent */
  mh->size_and_flags = 0xf008;
  while (mh->size_and_flags == 0xf008)
    ;

  next_message_start = 0;
  /* Enter main message loop */
  int i;
  for (i = 0; ;i++) {
    assert(next_message_start % CACHE_LINE_SIZE == 0);
    mh = td->data + mask_ring_index(next_message_start);
    sz = wait_for_message_ready(mh, MH_FLAG_READY);
    if (sz & MH_FLAG_STOP) /* End of test */
      break;
    sz &= ~MH_FLAGS;
    if(sz != td->size) {
      printf("%d %d %ld\n", sz, td->size, next_message_start);
      assert(0);
    }
    consume_message(td->data, next_message_start + sizeof(struct msg_header), sz, buf);

    set_message_ready(mh, sz);

    next_message_start += sz + sizeof(struct msg_header);
  }
}

static void
run_parent(test_data *td)
{
  volatile struct msg_header *mh = td->data;
  volatile struct msg_header *mh2;
  unsigned long next_tx_offset;
  unsigned long first_unacked_msg;
  char *buf = xmalloc(td->size);

  assert(td->size < ring_size - sizeof(struct msg_header));

  /* Wait for child to show up. */
  while (mh->size_and_flags != 0xf008)
    ;
  mh->size_and_flags = 0;

  next_tx_offset = 0;
  first_unacked_msg = 0;

  /* Round up to multiple of cache line size, for sanity. */
  td->size = td->size + CACHE_LINE_SIZE - 1;
  td->size -= td->size % CACHE_LINE_SIZE;

  thr_test(
    do {
      /* Check for available ring space (eom = end of message) */
      unsigned long eom = next_tx_offset + td->size + sizeof(struct msg_header) * 2;
      while (eom - first_unacked_msg > ring_size) {
	int size;
	mh = td->data + mask_ring_index(first_unacked_msg);
	size = wait_for_message_ready(mh, 0);
	size &= ~MH_FLAGS;
	first_unacked_msg += size + sizeof(struct msg_header);
      }

      /* Send message */
      mh = td->data + mask_ring_index(next_tx_offset);
      if(td->mode == MODE_DATAINPLACE) {
	set_message(td->data, next_tx_offset + sizeof(struct msg_header), td->size, i);
      }
      else {
	if(td->mode == MODE_DATAEXT)
	  memset(buf, i, td->size);
	copy_message(td->data, next_tx_offset + sizeof(struct msg_header), td->size, buf);
      }

      /* Make sure that the size field in the *next* message is clear
	 before setting the size field in *this* message.  That makes
	 sure that the receiver stops and spins in the right place,
	 rather than wandering off into la-la land if it picks up a
	 stale message. */
      mh2 = td->data + mask_ring_index(next_tx_offset + td->size + sizeof(struct msg_header));
      mh2->size_and_flags = 0;

      set_message_ready(mh, td->size | MH_FLAG_READY);

      next_tx_offset += td->size + sizeof(struct msg_header);
    } while(0),
    do {
      /* Wait for child to acknowledge receipt of all messages */
      while (first_unacked_msg != next_tx_offset) {
	int size;
	mh = td->data + mask_ring_index(first_unacked_msg);
	size = wait_for_message_ready(mh, 0);
	size &= ~MH_FLAGS;
	first_unacked_msg += size + sizeof(struct msg_header);
      }
    } while (0),
    td);

  /* Tell child to go away */
  mh = td->data + mask_ring_index(next_tx_offset);
  mh->size_and_flags = MH_FLAG_READY | MH_FLAG_STOP;
}

#ifdef USE_MWAIT
static void
cpuid(int leaf, unsigned long *a, unsigned long *b, unsigned long *c, unsigned long *d)
{
  unsigned long _a, _b, _c, _d;
  asm ("cpuid"
       : "=a" (_a), "=b" (_b), "=c" (_c), "=d" (_d)
       : "0" (leaf)
       );
  if (a)
    *a = _a;
  if (b)
    *b = _b;
  if (c)
    *c = _c;
  if (d)
    *d = _d;
}

static void
check_monitor_line_size(void)
{
  unsigned long a, b, c;
  cpuid(5, &a, &b, NULL, NULL);
  assert(a == b);
  assert(a == CACHE_LINE_SIZE);
  cpuid(1, NULL, NULL, &c, NULL);
  printf("Available: %d\n", !!(c & (1 << 3)));
}
#else
static void
check_monitor_line_size(void)
{
}
#endif

int
main(int argc, char *argv[])
{
  test_t t = { test_name, init_test, run_parent, run_child };
  char *ring_order = getenv("MEMPIPE_RING_ORDER");
  check_monitor_line_size();
  if (ring_order) {
    int as_int;
    if (sscanf(ring_order, "%d", &as_int) != 1)
      err(1, "MEMPIPE_RING_ORDER must be an integer");
    if (as_int < 0 || as_int > 15)
      errx(1, "MEMPIPE_RING_ORDER must be between 0 and 15");
    nr_shared_pages = 1 << as_int;
  }
  run_test(argc, argv, &t);
  return 0;
}
