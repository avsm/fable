/*
 * Copyright (c) 2011 Anil Madhavapeddy <anil@recoil.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>

typedef struct {
  int num;
  int size;
  size_t count;
  bool per_iter_timings;
  void *data;
  const char *output_dir;
  const char *name;
} test_data;

typedef struct {
  const char *name;
  void (*init_test)(test_data *);
  void (*run_parent)(test_data *);
  void (*run_child)(test_data *);
} test_t;

void run_test(int argc, char *argv[], test_t *test);

static inline unsigned long
rdtsc(void)
{
  unsigned long a, d;
  asm volatile("rdtsc"
	       : "=a" (a), "=d" (d)
	       );
  return (d << 32) | a;
}
void dump_tsc_counters(test_data *td, unsigned long *counts, int nr_samples);

void logmsg(test_data *td,
	    const char *file,
	    const char *fmt,
	    ...)
  __attribute__((format (printf, 3, 4)));

#define lat_or_thr_test(is_lat, body, finish, td)			\
  do {									\
    struct timeval start;						\
    struct timeval stop;						\
    unsigned long *iter_cycles;						\
    unsigned long delta;						\
    int i;								\
    									\
    /* calm compiler */							\
    iter_cycles = NULL;							\
									\
    if (td->per_iter_timings) {						\
      iter_cycles = calloc(sizeof(iter_cycles[0]), td->count);		\
      if (!iter_cycles)							\
	err(1, "calloc");						\
    }									\
									\
    gettimeofday(&start, NULL);						\
    if (!td->per_iter_timings) {					\
      for (i = 0; i < td->count; i++) {					\
	body;								\
      }									\
    } else {								\
      for (i = 0; i < td->count; i++) {					\
	unsigned long t = rdtsc();					\
	body;								\
	iter_cycles[i] = rdtsc() - t;					\
      }									\
    }									\
    finish;								\
    gettimeofday(&stop, NULL);						\
									\
    delta = ((stop.tv_sec - start.tv_sec) * (int64_t) 1000000 +		\
	     stop.tv_usec - start.tv_usec);				\
									\
    if (is_lat)								\
      logmsg(td,							\
	     "headline",						\
	     "%s %d %" PRId64 " %fs\n", td->name, td->size, td->count,	\
	     delta / (td->count * 1e6));				\
    else								\
      logmsg(td,							\
	     "headline",						\
	     "%s %d %" PRId64 " %" PRId64 " Mbps\n", td->name, td->size, \
	     td->count,							\
	     ((((td->count * (int64_t)1e6) / delta) * td->size * 8) / (int64_t) 1e6)); \
									\
    if (td->per_iter_timings)						\
      dump_tsc_counters(td, iter_cycles, td->count);			\
  } while (0)

#define latency_test(body, td)			                        \
  lat_or_thr_test(1, body, do {} while (0), td)
#define thr_test(body, finish, td)	                                \
  lat_or_thr_test(0, body, finish, td)
