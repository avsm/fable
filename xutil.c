/* 
    Copyright (c) 2011 Anil Madhavapeddy <anil@recoil.org>
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
#include <sys/time.h>
#include <assert.h>
#include <unistd.h>
#include <math.h>
#include <sched.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <err.h>
#include <inttypes.h>
#include "atomicio.h"
#include "xutil.h"
#include "test.h"

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

static double
point_to_percentile(const struct summary_stats *ss, double point)
{
  double y1, y2, num, denum;
  int low, high;
  int probe;

  if (point < ss->data[0])
    return 0;
  else if (point > ss->data[ss->nr_items-1])
    return 100;
  low = 0;
  high = ss->nr_items;
  while (low + 1 < high) {
    /* Invariant: everything in slots before @low is less than @point,
       everything in slots at or after @high is greater than
       @point. */
    probe = (high + low) / 2;
    assert(probe != low);
    if (point > ss->data[probe]) {
      low = probe + 1;
    } else if (point < ss->data[probe]) {
      high = probe;
    } else {
      /* The probe is now in the range of data which is equal to
	 point. */
      goto probe_is_point;
    }
  }
  if (high == low + 1) {
    if (point < ss->data[low]) {
      assert(low != 0);
      assert(point > ss->data[low-1]);
      low--;
      high--;
    }
    if (ss->data[low] == point) {
      probe = low;
      goto probe_is_point;
    } else if (ss->data[high] == point) {
      probe = high;
      goto probe_is_point;
    } else {
      goto linear_interpolate;
    }
  } else {
    assert(high == low);
    if (low == 0) {
      return 0;
    } else {
      low = high - 1;
      goto linear_interpolate;
    }
  }

 probe_is_point:
  low = probe;
  while (low >= 0 && ss->data[low] == point)
    low--;
  high = probe;
  while (high < ss->nr_items && ss->data[high] == point)
    high++;
  return (high + low) * 50.0 / ss->nr_items;

 linear_interpolate:
  y1 = ss->data[low];
  y2 = ss->data[high];
  num = (point + y2 * low - high * y1) * 100.0 / ss->nr_items;
  denum = y2 - y1;
  if (fabs(denum / num) < 0.01) {
    /* The two points we're trying to interpolate between are so close
       together that we risk numerical error, so we can't use the
       normal formula.  Fortunately, if they're that close together
       then it doesn't really matter, and we can use a simple
       average. */
    return (low + high) * 50.0 / ss->nr_items;
  } else {
    return num / denum;
  }
}

static void
print_summary_stats(const struct summary_stats *ss)
{
  double sd_percentiles[7];
  int i;

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

  /* Also look at how deltas from the mean, in multiples of the SD,
     map onto percentiles, to get more hints about non-normality. */
  for (i = 0; i < 7; i++) {
    double point = ss->mean + ss->sample_sd * (i - 3);
    sd_percentiles[i] = point_to_percentile(ss, point);
  }
  printf("\tSD percentiles: -3 -> %f%%, -2 -> %f%%, -1 -> %f%%, 0 -> %f%%, 1 -> %f%%, 2 -> %f%%, 3 -> %f%%\n",
	 sd_percentiles[0],
	 sd_percentiles[1],
	 sd_percentiles[2],
	 sd_percentiles[3],
	 sd_percentiles[4],
	 sd_percentiles[5],
	 sd_percentiles[6]);
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
  int discard;

  /* Discard the first few samples, so as to avoid startup
     transients. */
  discard = nr_samples / 20;
  counts += discard;
  nr_samples -= discard;

  for (i = 0; i < nr_samples; i++)
    times[i] = counts[i] / clock_freq;

  printf("By tenths of total run:\n");
  for (i = 0; i < 10; i++) {
    struct summary_stats stats;
    int start = (nr_samples * i) / 10;
    int end = (nr_samples * (i+1)) / 10;
    qsort(times + start, end - start, sizeof(times[0]), compare_double);
    calc_summary_stats(times + start, end - start, &stats);
    printf("Slice %d/10:\n", i);
    print_summary_stats(&stats);
  }

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

static void
help(char *argv[])
{
  fprintf(stderr, "Usage:\n%s [-h] [-2] [-p <num] [-t] [-s <bytes>] [-c <num>]\n", argv[0]);
  fprintf(stderr, "-h: show this help message\n");
  fprintf(stderr, "-2: force one of the processes onto a separate CPU\n");
  fprintf(stderr, "-p: number of parallel tests to run\n");
  fprintf(stderr, "-t: use high-res TSC to get more accurate results\n");
  fprintf(stderr, "-s: Size of each packet\n");
  fprintf(stderr, "-c: Number of iterations\n");
  exit(1);
}
 
void
parse_args(int argc, char *argv[], bool *per_iter_timings, int *size, size_t *count, bool *separate_cpu, int *parallel)
{
  int opt;
  *per_iter_timings = false;
  *separate_cpu = false;
  *parallel = 1;
  *size = 1024;
  *count = 100;
  while((opt = getopt(argc, argv, "h?tp:2s:c:")) != -1) {
    switch(opt) {
     case 't':
      *per_iter_timings = true;
      break;
     case 'p':
      *parallel = atoi(optarg);
      break;
     case '2':
      *separate_cpu = true;
      break;
     case 's':
      *size = atoi(optarg);
      break;
     case 'c':
      *count = atoi(optarg);
      break;
     case '?':
     case 'h':
      help(argv);
    default:
      fprintf(stderr, "unknown command-line option '%c'", opt);
      help(argv);
    }
  }
  fprintf(stderr, "size %d count %" PRId64 " separate_cpu %d parallel %d tsc %d\n", *size, *count, *separate_cpu, *parallel, *per_iter_timings);
}

void
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
