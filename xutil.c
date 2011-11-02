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
#include <sys/time.h>
#include <unistd.h>
#include <math.h>
#include <sched.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <err.h>
#include "atomicio.h"
#include "xutil.h"

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
xwrite(int fd, const void *buf, size_t count)
{
  ssize_t r;
  r = atomicio(vwrite, fd, (void *)buf, count);
  if (r != count)
    err(1, "xwrite");
}

struct summary_stats {
  const double *data;
  int nr_items;

  double mean;
  double sample_sd;
  double sample_skew;
  double sample_kurtosis;
};

static double
get_tsc_freq(void)
{
  double estimates[5];
  int nr_estimates;
  struct timeval start;
  struct timeval now;
  unsigned long start_tsc;
  unsigned long end_tsc;
  double low_estimate, high_estimate;
  double total;
  int i;

  while (1) {
    for (nr_estimates = 0; nr_estimates < 5; nr_estimates++) {
      gettimeofday(&start, NULL);
      start_tsc = rdtsc();
      do {
	gettimeofday(&now, NULL);
	now.tv_sec -= start.tv_sec;
	now.tv_usec -= start.tv_usec;
	if (now.tv_usec < 0) {
	  now.tv_sec--;
	  now.tv_usec += 1e6;
	}
      } while (now.tv_usec < 1e4);
      end_tsc = rdtsc();
      estimates[nr_estimates] = (end_tsc - start_tsc) / (now.tv_usec * 1e-6);
    }

    /* Check for anomalies */
    low_estimate = estimates[0];
    high_estimate = estimates[0];
    for (i = 1; i < nr_estimates; i++) {
      if (estimates[i] < low_estimate)
	low_estimate = estimates[i];
      if (estimates[i] > high_estimate)
	high_estimate = estimates[i];
    }
    /* Accept if range of estimates is less than 1% */
    if (high_estimate < low_estimate * 1.01)
      break;
    /* Otherwise try again */
  }

  /* Use an arithmetic mean.  Should arguably be using harmonic mean
     here, but since the thinks we're averaging vary by less than 1%
     it shouldn't make much difference. */
  total = 0;
  for (i = 0; i < nr_estimates; i++)
    total += estimates[i];
  return total / nr_estimates;
}

static int
compare_double(const void *_a, const void *_b)
{
  const double *a = _a;
  const double *b = _b;
  if (*a < *b)
    return -1;
  else if (*a == *b)
    return 0;
  else
    return 1;
}

static void
calc_summary_stats(const double *data, int nr_items, struct summary_stats *out)
{
  /* On-line calculation of mean, variance, skew and kurtosis
     lifted straight from wikipedia. */
  double mean = 0;
  double m2 = 0;
  double m3 = 0;
  double m4 = 0;
  double delta;
  double delta_n;
  double variance;
  double sd;
  double skew;
  double kurtosis;
  double n;
  int i;

  for (i = 0; i < nr_items; i++) {
    n = i + 1;
    delta = data[i] - mean;
    delta_n = delta / n;
    mean = (mean * i) / n + data[i]/n;
    m4 = m4 + delta_n * delta_n * delta_n * delta * (n - 1) * (n * n - 3 * n + 3) + 6 * delta_n * delta_n * m2 - 4 * delta_n * m3;
    m3 = m3 + delta_n * delta_n * delta * (n - 1) * (n - 2) - 3 * delta_n * m2;
    m2 = m2 + delta_n * delta * (n - 1);
  }

  variance = m2 / nr_items;
  sd = sqrt(variance);
  skew = m3/(nr_items * sd * sd * sd);
  kurtosis = nr_items * m4 / (m2*m2) - 3;

  out->mean = mean;
  out->sample_sd = sd;
  out->sample_skew = skew;
  out->sample_kurtosis = kurtosis;

  out->data = data;
  out->nr_items = nr_items;
}

static void
print_summary_stats(const struct summary_stats *ss)
{
  printf("\tMean %e, sample sd %e, sample skew %e, sample kurtosis %e\n",
	 ss->mean, ss->sample_sd, ss->sample_skew, ss->sample_kurtosis);
  printf("\tQuintiles: %e, %e, %e, %e, %e, %e\n",
	 ss->data[0],
	 ss->data[ss->nr_items / 5],
	 ss->data[ss->nr_items * 2 / 5],
	 ss->data[ss->nr_items * 3 / 5],
	 ss->data[ss->nr_items * 4 / 5],
	 ss->data[ss->nr_items - 1]);
  printf("\t5%% %e, median %e, 95%% %e\n",
	 ss->data[ss->nr_items / 20],
	 ss->data[ss->nr_items / 2],
	 ss->data[ss->nr_items * 19 / 20]);
}

void
summarise_tsc_counters(unsigned long *counts, int nr_samples)
{
  double *times = xmalloc(sizeof(times[0]) * nr_samples);
  double clock_freq = get_tsc_freq();
  struct summary_stats whole_dist_stats;
  struct summary_stats low_outliers;
  struct summary_stats high_outliers;
  struct summary_stats excl_outliers;
  int i;
  int low_thresh, high_thresh;

  for (i = 0; i < nr_samples; i++)
    times[i] = counts[i] / clock_freq;
  qsort(times, nr_samples, sizeof(times[0]), compare_double);

  calc_summary_stats(times, nr_samples, &whole_dist_stats);

  printf("Distribution of all values:\n");
  print_summary_stats(&whole_dist_stats);

#define OUTLIER 10
  low_thresh = nr_samples / OUTLIER;
  high_thresh = nr_samples - nr_samples / OUTLIER;
#undef OUTLIER
  if (low_thresh >= high_thresh ||
      low_thresh == 0 ||
      high_thresh == nr_samples)
    return;
  calc_summary_stats(times, low_thresh, &low_outliers);
  calc_summary_stats(times + low_thresh, high_thresh - low_thresh, &excl_outliers);
  calc_summary_stats(times + high_thresh, nr_samples - high_thresh, &high_outliers);

  printf("Low outliers:\n");
  print_summary_stats(&low_outliers);

  printf("Bulk distribution:\n");
  print_summary_stats(&excl_outliers);

  printf("High outliers:\n");
  print_summary_stats(&high_outliers);

  free(times);
}

void
parse_args(int argc, char *argv[], bool *per_iter_timings, int *size, int64_t *count)
{
  char *argv0 = argv[0];
  *per_iter_timings = false;
  if (argc == 4 && !strcmp(argv[1], "-p")) {
    *per_iter_timings = true;
    argc--;
    argv++;
  }

  if (argc != 3) {
    printf ("usage: %s {-p} <message-size> <roundtrip-count>\n", argv0);
    exit(1);
  }

  *size = atoi(argv[1]);
  *count = atol(argv[2]);
}

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
