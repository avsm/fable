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

#include <stdbool.h>

void * xmalloc(size_t);
void xread(int, void *, size_t);
void xwrite(int, const void *, size_t);

void setaffinity(int);
void parse_args(int argc, char *argv[], bool *per_iter_timings, int *size, size_t *count, bool *separate_cpu, int *parallel);

static inline unsigned long
rdtsc(void)
{
  unsigned long a, d;
  asm volatile("rdtsc"
	       : "=a" (a), "=d" (d)
	       );
  return (d << 32) | a;
}
void summarise_tsc_counters(unsigned long *counts, int nr_samples);

#define latency_test(name, body, per_iter_timings, size, count)		\
  do {									\
    struct timeval start;						\
    struct timeval stop;						\
    unsigned long *iter_cycles;						\
    unsigned long delta;						\
									\
    /* calm compiler */							\
    iter_cycles = NULL;							\
									\
    if (per_iter_timings) {						\
      iter_cycles = calloc(sizeof(iter_cycles[0]), count);		\
      if (!iter_cycles)							\
	err(1, "calloc");						\
    }									\
									\
    gettimeofday(&start, NULL);						\
    if (!per_iter_timings) {						\
      for (i = 0; i < count; i++) {					\
	body;								\
      }									\
    } else {								\
      for (i = 0; i < count; i++) {					\
	unsigned long t = rdtsc();					\
	body;								\
	iter_cycles[i] = rdtsc() - t;					\
      }									\
    }									\
    gettimeofday(&stop, NULL);						\
									\
    delta = ((stop.tv_sec - start.tv_sec) * (int64_t) 1000000 +		\
	     stop.tv_usec - start.tv_usec);				\
									\
    printf("%s %d %" PRId64 " %f\n", name, size, count,			\
	   delta / (count * 1e6));					\
									\
    if (per_iter_timings)						\
      summarise_tsc_counters(iter_cycles, count);			\
  } while (0)


