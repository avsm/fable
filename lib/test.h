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

#define PRODUCE_GLIBC_MEMSET 1
#define PRODUCE_STOS_MEMSET 2
#define PRODUCE_LOOP 3

typedef struct {
  int num;
  int size;
  size_t count;
  bool per_iter_timings;
  void *data;
  const char *output_dir;
  const char *name;
  int write_in_place;
  int read_in_place;
  int produce_method;
  int do_verify;
  int first_core;
  int second_core;
  int numa_node;
} test_data;

typedef struct {
  const char *name;
  int is_latency_test;
  void (*init_test)(test_data *);
  void (*init_parent)(test_data *);
  void (*finish_parent)(test_data *);
  void (*init_child)(test_data *);
  void (*finish_child)(test_data *);
  struct iovec* (*get_write_buffer)(test_data *, int size, int* n_vecs);
  void (*release_write_buffer)(test_data *, struct iovec* vecs, int n_vecs);
  struct iovec* (*get_read_buffer)(test_data *, int size, int* n_vecs);
  void (*release_read_buffer)(test_data *, struct iovec* vecs, int n_vecs);
  void (*parent_ping)(test_data *);
  void (*child_ping)(test_data *);
} test_t;

static inline unsigned long
rdtsc(void)
{
  unsigned long a, d;
  asm volatile("rdtsc"
	       : "=a" (a), "=d" (d)
	       );
  return (d << 32) | a;
}

void run_test(int argc, char *argv[], test_t *test);

void dump_tsc_counters(test_data *td, unsigned long *counts, int nr_samples);

void logmsg(test_data *td,
	    const char *file,
	    const char *fmt,
	    ...)
  __attribute__((format (printf, 3, 4)));
