/* Another kind of shared memory test.  The idea here is that we have
   a shared-memory region and a malloc-like thing for allocating from
   it, and we also have a couple of pipes.  Messages are sent by
   allocating a chunk of the shared region and then sending an extent
   through the pipe.  Once the receiver is finished with the message,
   they send another extent back through the other pipe saying that
   they're done. */
#include <sys/poll.h>
#include <sys/time.h>
#include <assert.h>
#include <err.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "test.h"
#include "xutil.h"

#define PAGE_ORDER 12
#define CACHE_LINE_SIZE 64
static unsigned ring_order = 14;
#define ring_size (1ul << (PAGE_ORDER + ring_order))

#define EXTENT_BUFFER_SIZE 4096

#define ALLOC_FAILED ((unsigned)-1)

#ifdef NDEBUG
#define DBG(x) do {} while (0)
#else
#define DBG(x) do { x; } while (0)
#endif

struct shmem_pipe {
	void *ring;
	struct alloc_node *first_alloc, *next_free_alloc, *last_freed_node;
#ifndef NDEBUG
	int nr_alloc_nodes;
#endif

	int child_to_parent_read, child_to_parent_write;
	int parent_to_child_read, parent_to_child_write;

	unsigned char rx_buf[EXTENT_BUFFER_SIZE];
	unsigned rx_buf_prod;
	unsigned rx_buf_cons;
};

struct extent {
	unsigned base;
	unsigned size;
#ifndef NDEBUG
	int sequence;
#endif
};

/* Our allocation structure is a simple linked list.  That's pretty
   stupid, *except* that the allocation pattern is almost always a
   very simple queue, so it becomes very simple.  i.e. we release
   stuff in a FIFO order wrt allocations, so we effectively just have
   one allocated region which loops around the shared area, which
   makes the linked list very short and everything is very easy. */
struct alloc_node {
	struct alloc_node *next, *prev;
	int is_free;
	unsigned long start;
	unsigned long end;
};

#ifndef NDEBUG
static void
sanity_check(const struct shmem_pipe *sp)
{
	const struct alloc_node *cursor;
	int found_nf = 0, found_lf = 0;
	int n = 0;
	assert(sp->first_alloc);
	assert(sp->first_alloc->start == 0);
	for (cursor = sp->first_alloc;
	     cursor;
	     cursor = cursor->next) {
		n++;
		if (cursor == sp->first_alloc)
			assert(!cursor->prev);
		else
			assert(cursor->prev);
		if (cursor->next)
			assert(cursor->next->prev == cursor);
		if (cursor->prev)
			assert(cursor->start == cursor->prev->end);
		if (cursor->prev)
			assert(cursor->is_free == !cursor->prev->is_free);
		if (!cursor->next)
			assert(cursor->end == ring_size);
		if (cursor == sp->next_free_alloc) {
			assert(!found_nf);
			found_nf = 1;
		}
		if (cursor == sp->last_freed_node) {
			assert(!found_lf);
			found_lf = 1;
		}
		assert(cursor->start < cursor->end);
	}
	if (!found_nf)
		assert(!sp->next_free_alloc);
	else
		assert(sp->next_free_alloc->is_free);
	if (!found_lf)
		assert(!sp->last_freed_node);
	else
		assert(sp->last_freed_node->is_free);
	assert(n == sp->nr_alloc_nodes);
}
#else
static void
sanity_check(const struct shmem_pipe *sp)
{
}
#endif

static unsigned
alloc_shared_space(struct shmem_pipe *sp, unsigned size)
{
	unsigned res;

	sanity_check(sp);

	/* Common case */
	if (sp->next_free_alloc &&
	    sp->next_free_alloc->end >= size + sp->next_free_alloc->start &&
	    sp->next_free_alloc->prev) {
	allocate_next_free:
		assert(!sp->next_free_alloc->prev->is_free);
		assert(sp->next_free_alloc->is_free);
		res = sp->next_free_alloc->start;
		sp->next_free_alloc->start += size;
		sp->next_free_alloc->prev->end += size;
		if (sp->next_free_alloc->start == sp->next_free_alloc->end) {
			if (sp->next_free_alloc->next) {
				assert(!sp->next_free_alloc->next->is_free);
				sp->next_free_alloc->prev->next = sp->next_free_alloc->next->next;
				sp->next_free_alloc->prev->end = sp->next_free_alloc->next->end;
				if (sp->next_free_alloc->next->next) {
					assert(sp->next_free_alloc->next->next->is_free);
					sp->next_free_alloc->next->next->prev = sp->next_free_alloc->prev;
				}
				struct alloc_node *p = sp->next_free_alloc->next->next;
				DBG(sp->nr_alloc_nodes--);
				free(sp->next_free_alloc->next);
				if (sp->next_free_alloc->next == sp->last_freed_node)
					sp->last_freed_node = NULL;
				sp->next_free_alloc->next = p;
			} else {
				if (sp->next_free_alloc->prev)
					sp->next_free_alloc->prev->next = NULL;
			}
			if (sp->first_alloc == sp->next_free_alloc) {
				assert(sp->next_free_alloc->next);
				assert(!sp->next_free_alloc->prev);
				sp->first_alloc = sp->next_free_alloc->next;
			}
			if (sp->next_free_alloc == sp->last_freed_node)
				sp->last_freed_node = NULL;
			DBG(sp->nr_alloc_nodes--);
			free(sp->next_free_alloc);
			sp->next_free_alloc = NULL;
		}
		sanity_check(sp);
		return res;
	}

	/* Slightly harder case: have to search the linked list */
	for (sp->next_free_alloc = sp->first_alloc;
	     sp->next_free_alloc &&
		     (!sp->next_free_alloc->is_free || sp->next_free_alloc->end - sp->next_free_alloc->start < size);
	     sp->next_free_alloc = sp->next_free_alloc->next)
		;
	if (!sp->next_free_alloc) {
		/* Shared area is full */
		return ALLOC_FAILED;
	}

	struct alloc_node *f = sp->next_free_alloc;
	assert(f->is_free);
	if (!f->prev) {
		/* Allocate the start of the arena. */
		assert(f->start == 0);
		assert(f == sp->first_alloc);
		if (f->end == size) {
			/* We're going to convert next_free_alloc to
			 * an in-use node.  This may involve forwards
			 * merging. */
			if (f->next) {
				struct alloc_node *t = f->next;
				assert(!t->is_free);
				f->end = t->end;
				f->next = t->next;
				if (f->next)
					f->next->prev = f;
				if (sp->last_freed_node == t)
					sp->last_freed_node = NULL;
				DBG(sp->nr_alloc_nodes--);
				free(t);
			}
			f->is_free = 0;
		} else {
			f = calloc(sizeof(struct alloc_node), 1);
			DBG(sp->nr_alloc_nodes++);
			f->next = sp->first_alloc;
			f->start = 0;
			f->end = size;
			assert(f->next);
			f->next->prev = f;
			f->next->start = size;
			sp->first_alloc = f;
		}
		if (sp->last_freed_node == sp->first_alloc)
			sp->last_freed_node = sp->first_alloc->next;
		if (sp->next_free_alloc == sp->first_alloc)
			sp->next_free_alloc = sp->first_alloc->next;
		sanity_check(sp);
		return 0;
	} else {
		goto allocate_next_free;
	}
}

static void
release_shared_space(struct shmem_pipe *sp, unsigned start, unsigned size)
{
	struct alloc_node *lan = sp->last_freed_node;
	assert(start <= ring_size);
	assert(start + size <= ring_size);
	assert(size > 0);
	sanity_check(sp);
	if (lan &&
	    lan->is_free &&
	    lan->end == start) {
		struct alloc_node *next;
	free_from_here:
		next = lan->next;
		assert(next);
		assert(!next->is_free);
		assert(next->start == start);
		assert(next->end >= start + size);
		next->start += size;
		lan->end += size;
		if (next->start == next->end) {
			/* We just closed a hole.  Previously, we had
			   LAN->next->X, where LAN is sp->last_freed_node,
			   next is some free region, and X is either
			   NULL or some allocated region.  next is now
			   zero-sized, so we want to remove it and
			   convert to LAN->X.  However, LAN and X are
			   the same type (i.e. both non-free), so we
			   can extend LAN to cover X and remove X as
			   well. */
			struct alloc_node *X = next->next;

			if (X) {
				/* Convert LAN->next->X->Y into
				   LAN->Y */
				struct alloc_node *Y = X->next;
				assert(X->is_free);
				if (Y) {
					assert(!Y->is_free);
					Y->prev = lan;
				}
				lan->end = X->end;
				lan->next = Y;
				if (X == sp->next_free_alloc)
					sp->next_free_alloc = lan;
				DBG(sp->nr_alloc_nodes--);
				free(X);
			} else {
				/* Just turn LAN->free1->NULL into
				   LAN->NULL */
				assert(lan->end == next->start);
				lan->next = NULL;
			}
			if (next == sp->next_free_alloc)
				sp->next_free_alloc = lan;
			DBG(sp->nr_alloc_nodes--);
			free(next);
		}
		sanity_check(sp);
		return;
	}

	/* More tricky case: we're freeing something which doesn't hit
	 * the cache. */
	for (lan = sp->first_alloc;
	     lan && (lan->end <= start || lan->start > start);
	     lan = lan->next)
		;
	assert(lan); /* Or else we're freeing something outside of the arena */
	assert(!lan->is_free); /* Or we have a double free */
	if (lan->start == start) {
		/* Free out the start of this block. */
		assert(!lan->is_free);
		if (lan->prev) {
			assert(lan->prev->is_free);
			assert(lan->prev->end == start);
			sp->last_freed_node = lan = lan->prev;
			goto free_from_here;
		}
		/* Turn the very start of the arena into a free
		 * block */
		assert(lan == sp->first_alloc);
		assert(start == 0);
		if (lan->end == size) {
			/* Easy: just convert the existing node to a
			 * free one. */
			lan->is_free = 1;
			if (lan->next && lan->next->is_free) {
				/* First node is now free, and the
				   second node already was -> merge
				   them. */
				struct alloc_node *t = lan->next;
				lan->end = t->end;
				lan->next = t->next;
				if (lan->next)
					lan->next->prev = lan;
				if (sp->last_freed_node == t)
					sp->last_freed_node = lan;
				if (sp->next_free_alloc == t)
					sp->next_free_alloc = lan;
				DBG(sp->nr_alloc_nodes--);
				free(t);
			}
			sanity_check(sp);
		} else {
			/* Need a new node in the list */
			lan = calloc(sizeof(*lan), 1);
			lan->is_free = 1;
			lan->end = size;
			sp->first_alloc->start = lan->end;
			sp->first_alloc->prev = lan;
			lan->next = sp->first_alloc;
			sp->first_alloc = lan;
			sp->last_freed_node = sp->first_alloc;
			DBG(sp->nr_alloc_nodes++);
			sanity_check(sp);
		}
		return;
	}
	assert(start < lan->end);
	assert(start + size <= lan->end);
	if (start + size == lan->end) {
		/* Free out the end of this block */
		if (lan->next) {
			assert(lan->next->is_free);
			lan->next->start -= size;
			lan->end -= size;
			assert(lan->end != lan->start);
		} else {
			struct alloc_node *t = calloc(sizeof(*lan), 1);
			t->prev = lan;
			t->is_free = 1;
			t->start = start;
			t->end = start + size;
			lan->next = t;
			lan->end = start;
			DBG(sp->nr_alloc_nodes++);
		}
		if (!sp->next_free_alloc)
			sp->next_free_alloc = lan->next;
		sp->last_freed_node = lan->next;
		sanity_check(sp);
		return;
	}

	/* Okay, this is the tricky case.  We have a single allocated
	   node, and we need to convert it into three: an allocated
	   node, a free node, and then another allocated node.  How
	   tedious. */
	struct alloc_node *a = calloc(sizeof(*a), 1);
	struct alloc_node *b = calloc(sizeof(*b), 1);

	a->next = b;
	a->prev = lan;
	a->is_free = 1;
	a->start = start;
	a->end = start + size;

	b->next = lan->next;
	b->prev = a;
	b->is_free = 0;
	b->start = start + size;
	b->end = lan->end;

	if (lan->next)
		lan->next->prev = b;
	lan->next = a;
	lan->end = start;

	DBG(sp->nr_alloc_nodes += 2);

	if (!sp->next_free_alloc)
		sp->next_free_alloc = a;

	/* And we're done. */
	sanity_check(sp);
}

static void
init_test(test_data *td)
{
	struct shmem_pipe *sp = calloc(sizeof(*sp), 1);
	int pip[2];
	sp->ring = establish_shm_segment(1 << ring_order);
	if (pipe(pip) < 0)
		err(1, "pipe()");
	sp->child_to_parent_read = pip[0];
	sp->child_to_parent_write = pip[1];
	if (pipe(pip) < 0)
		err(1, "pipe()");
	sp->parent_to_child_read = pip[0];
	sp->parent_to_child_write = pip[1];
	td->data = sp;

	sp->first_alloc = calloc(sizeof(*sp->first_alloc), 1);
	sp->first_alloc->is_free = 1;
	sp->first_alloc->end = ring_size;
	DBG(sp->nr_alloc_nodes = 1);
}

#ifdef SOS22_MEMSET
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
#define real_memset mymemset
#else
#define real_memset memset
#endif

static void
set_message(void *data, unsigned size, int byte)
{
	real_memset(data, byte, size);
}

static void
copy_message(void *data, const void *inbuf, unsigned size)
{
	memcpy(data, inbuf, size);
}

static void
populate_message(void *buf, int size, int mode, void *local_buf)
{
	static unsigned cntr;
	switch (mode) {
	case MODE_DATAINPLACE:
		set_message(buf, size, cntr);
		break;
	case MODE_DATAEXT:
		memset(local_buf, cntr, size);
	case MODE_NODATA:
		copy_message(buf, local_buf, size);
		break;
	default:
		abort();
	}
	cntr++;
}

static void
consume_message(const void *buf, int size, void *outbuf)
{
	memcpy(outbuf, buf, size);
}

static void
run_child(test_data *td)
{
	struct shmem_pipe *sp = td->data;
	unsigned char incoming[EXTENT_BUFFER_SIZE];
	unsigned char outgoing[EXTENT_BUFFER_SIZE];
	struct pollfd pfd[2];
	int incoming_bytes = 0;
	unsigned outgoing_cons = 0, outgoing_prod = 0;
	int i;
	int j;
	int k;
	bool write_closed = false;
	int incoming_sequence = 0;
	int outgoing_sequence = 0xf0010000;
	void *rx_buf = malloc(td->size);

	close(sp->child_to_parent_read);
	close(sp->parent_to_child_write);
	while (1) {
		i = 0;
		if (incoming_bytes < sizeof(incoming) &&
		    outgoing_prod - outgoing_cons < sizeof(outgoing) - sizeof(struct extent)) {
			pfd[i].fd = sp->parent_to_child_read;
			pfd[i].events = POLLIN|POLLHUP|POLLRDHUP;
			pfd[i].revents = 0;
			i++;
		}
		if (outgoing_prod != outgoing_cons) {
			pfd[i].fd = sp->child_to_parent_write;
			pfd[i].events = POLLOUT;
			pfd[i].revents = 0;
			i++;
		}
		assert(i != 0);
		j = poll(pfd, i, -1);
		if (j < 0)
			err(1, "poll");
		for (j = 0; j < i; j++) {
			if (!pfd[j].revents)
				continue;
			if (pfd[j].fd == sp->parent_to_child_read) {
				k = read(sp->parent_to_child_read,
					 (void *)incoming + incoming_bytes,
					 sizeof(incoming) - incoming_bytes);
				if (k == 0)
					goto eof;
				if (k < 0)
					err(1, "child read");
				incoming_bytes += k;
			} else {
				int to_copy;

				assert(pfd[j].fd == sp->child_to_parent_write);
				to_copy = outgoing_prod - outgoing_cons;
				if (outgoing_cons / sizeof(outgoing) != outgoing_prod / sizeof(outgoing)) {
					assert(to_copy >= sizeof(outgoing) - (outgoing_cons % sizeof(outgoing)));
					to_copy = sizeof(outgoing) - (outgoing_cons % sizeof(outgoing));
				}
				k = write(sp->child_to_parent_write,
					  outgoing + outgoing_cons % sizeof(outgoing),
					  to_copy);
				if (k < 0)
					err(1, "child write");
				if (k == 0) {
					printf("Child write pipe is closed!\n");
					abort();
					write_closed = true;
				}
				outgoing_cons += k;
			}
		}

		for (i = 0;
		     i < incoming_bytes / sizeof(struct extent) &&
			     outgoing_prod - outgoing_cons < sizeof(outgoing) - sizeof(struct extent);
		     i++) {
			struct extent *inc = &((struct extent *)incoming)[i];
			struct extent out;
			assert(inc->base <= ring_size);
			assert(inc->base + inc->size <= ring_size);
			assert(inc->sequence == incoming_sequence);
			incoming_sequence++;
			consume_message(sp->ring + inc->base, inc->size, rx_buf);
			out = *inc;
#ifndef NDEBUG
			out.sequence = outgoing_sequence;
#endif

			if ((outgoing_prod + sizeof(out)) / sizeof(outgoing) == outgoing_prod / sizeof(outgoing)) {
				memcpy(outgoing + (outgoing_prod % sizeof(outgoing)),
				       &out,
				       sizeof(out));
			} else {
				memcpy(outgoing + (outgoing_prod % sizeof(outgoing)),
				       &out,
				       sizeof(outgoing) - (outgoing_prod % sizeof(outgoing)));
				memcpy(outgoing,
				       (void *)((unsigned long)&out + sizeof(outgoing) - (outgoing_prod % sizeof(outgoing))),
				       sizeof(out) - (sizeof(outgoing) - (outgoing_prod % sizeof(outgoing))));
			}

			outgoing_sequence++;
			outgoing_prod += sizeof(out);
		}
		memmove(incoming, incoming + i * sizeof(struct extent), incoming_bytes - i * sizeof(struct extent));
		incoming_bytes -= i * sizeof(struct extent);

		if (write_closed)
			outgoing_prod = outgoing_cons;
	}

eof:
	close(sp->child_to_parent_write);
	close(sp->parent_to_child_read);
	return;
}

static void
wait_for_returned_buffers(struct shmem_pipe *sp)
{
	int r;
	int s;
	static int total_read;
	static int sequence = 0xf0010000;

	s = read(sp->child_to_parent_read, sp->rx_buf + sp->rx_buf_prod, sizeof(sp->rx_buf) - sp->rx_buf_prod);
	if (s < 0)
		err(1, "error reading in parent");
	total_read += s;
	sp->rx_buf_prod += s;
	for (r = 0; r < sp->rx_buf_prod / sizeof(struct extent); r++) {
		struct extent *e = &((struct extent *)sp->rx_buf)[r];
		assert(e->sequence == sequence);
		sequence++;
		release_shared_space(sp, e->base, e->size);
	}
	if (sp->rx_buf_prod != r * sizeof(struct extent))
		memmove(sp->rx_buf,
			sp->rx_buf + sp->rx_buf_prod - (sp->rx_buf_prod % sizeof(struct extent)),
			sp->rx_buf_prod % sizeof(struct extent));
	sp->rx_buf_prod %= sizeof(struct extent);
}

static void
send_a_message(struct shmem_pipe *sp, int message_size, int mode, void *buf)
{
#ifndef NDEBUG
	static int sequence;
#endif
	unsigned long offset;
	struct extent ext;

	while ((offset = alloc_shared_space(sp, message_size)) == ALLOC_FAILED)
		wait_for_returned_buffers(sp);

	populate_message(sp->ring + offset, message_size, mode, buf);

#ifndef NDEBUG
	ext.sequence = sequence;
	sequence++;
#endif

	ext.base = offset;
	ext.size = message_size;

	xwrite(sp->parent_to_child_write, &ext, sizeof(ext));

	assert(sp->nr_alloc_nodes <= 3);
}

static void
flush_buffers(struct shmem_pipe *sp)
{
	char buf[1024];
	int r;

	close(sp->parent_to_child_write);
	/* Wait for the other pipe to drain, which confirms receipt of
	   all messages. */
	while (1) {
		r = read(sp->child_to_parent_read, buf, sizeof(buf));
		if (r == 0)
			break;
		if (r < 0)
			err(1, "reading in parent for child shutdown");
	}
	close(sp->child_to_parent_read);
}

static void
run_parent(test_data *td)
{
	struct shmem_pipe *sp = td->data;
	void *local_buf = malloc(td->size);

	close(sp->child_to_parent_write);
	close(sp->parent_to_child_read);

	thr_test(
		send_a_message(sp, td->size, td->mode, local_buf),
		flush_buffers(sp),
		td);
}

int
main(int argc, char *argv[])
{
	test_t t = { "shmem_pipe"
#ifdef SOS22_MEMSET
		     "_sos22"
#endif
		     , init_test, run_parent, run_child };
	char *_ring_order = getenv("SHMEM_RING_ORDER");
	if (_ring_order) {
		int as_int;
		if (sscanf(_ring_order, "%d", &as_int) != 1)
			err(1, "SHMEM_RING_ORDER must be an integer");
		if (as_int < 0 || as_int > 15)
			errx(1, "SHMEM_RING_ORDER must be between 0 and 15");
		ring_order = as_int;
	}
	run_test(argc, argv, &t);
	return 0;
}
