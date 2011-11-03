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
#include <sys/time.h>
#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "test.h"
#include "xutil.h"

#define PAGE_SIZE 4096
#define CACHE_LINE_SIZE 64
#define NR_SHARED_PAGES 512
#define RING_SIZE (PAGE_SIZE * NR_SHARED_PAGES)

struct msg_header {
  int size;
  int pad[CACHE_LINE_SIZE / sizeof(int) - 1];
};

static void
init_test(test_data *td)
{
  td->data = establish_shm_segment(NR_SHARED_PAGES);
}

static void
consume_message(const void *data, unsigned long offset, unsigned size,
		void *outbuf)
{
  offset %= RING_SIZE;
  if (offset + size <= RING_SIZE) {
    memcpy(outbuf, data + offset, size);
  } else {
    memcpy(outbuf, data + offset, RING_SIZE - offset);
    memcpy(outbuf + RING_SIZE - offset, data, size - (RING_SIZE - offset));
  }
}

static void
populate_message(void *data, unsigned long offset, unsigned size, const void *inbuf)
{
  offset %= RING_SIZE;
  if (offset + size <= RING_SIZE) {
    memcpy(data + offset, inbuf, size);
  } else {
    memcpy(data + offset, inbuf, RING_SIZE - offset);
    memcpy(data, inbuf + (RING_SIZE - offset), size - (RING_SIZE - offset));
  }
}

static void
run_child(test_data *td)
{
  unsigned long next_message_start;
  volatile struct msg_header *mh = td->data;
  int sz;
  char *buf = xmalloc(td->size);

  /* Sync up with parent */
  mh->size = 0xf001;
  while (mh->size == 0xf001)
    ;

  next_message_start = 0;
  /* Enter main message loop */
  while (1) {
    mh = td->data + (next_message_start % RING_SIZE);
    while (mh->size <= 0)
      ;
    sz = mh->size;
    if (sz == 1) /* End of test; normal messages are multiples of
		    cache line size. */
      break;
    assert(sz == td->size);
    consume_message(td->data, next_message_start + sizeof(struct msg_header), sz, buf);
    mh->size = -sz;
    next_message_start += sz + sizeof(struct msg_header);
  }
}

static void
run_parent(test_data *td)
{
  volatile struct msg_header *mh = td->data;
  unsigned long next_tx_offset;
  unsigned long first_unacked_msg;
  char *buf = xmalloc(td->size);

  assert(td->size < RING_SIZE - sizeof(struct msg_header));

  /* Wait for child to show up. */
  while (mh->size != 0xf001)
    ;
  mh->size = 0;

  next_tx_offset = 0;
  first_unacked_msg = 0;

  /* Round up to multiple of cache line size, for sanity. */
  td->size = td->size + CACHE_LINE_SIZE - 1;
  td->size -= td->size % CACHE_LINE_SIZE;

  thr_test("mempipe_thr",
    do {
      /* Check for available ring space (eom = end of message) */
      unsigned long eom = next_tx_offset + td->size + sizeof(struct msg_header);
      while (eom - first_unacked_msg > RING_SIZE) {
	int size;
	mh = td->data + (first_unacked_msg % RING_SIZE);
	do {
	  size = mh->size;
	} while (size > 0);
	assert(size < 0);
	assert(size % CACHE_LINE_SIZE == 0);
	first_unacked_msg += -size + sizeof(struct msg_header);
      }
      /* Send message */
      mh = td->data + (next_tx_offset % RING_SIZE);
      populate_message(td->data, next_tx_offset + sizeof(struct msg_header), td->size, buf);
      mh->size = td->size;
      next_tx_offset += td->size + sizeof(struct msg_header);
    } while(0),
    do {
      /* Wait for child to acknowledge receipt of all messages */
      while (first_unacked_msg != next_tx_offset) {
	int size;
	mh = td->data + (first_unacked_msg % RING_SIZE);
	do {
	  size = mh->size;
	} while (size > 0);
	first_unacked_msg += -size + sizeof(struct msg_header);
      }
    } while (0),
    td);

  /* Tell child to go away */
  mh = td->data + (next_tx_offset % RING_SIZE);
  mh->size = 1;
}

int
main(int argc, char *argv[])
{
  test_t t = { init_test, run_parent, run_child };
  run_test(argc, argv, &t);
  return 0;
}
