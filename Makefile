CFLAGS = -g -Wall -O3 -D_GNU_SOURCE
LDFLAGS+=-lm

.PHONY: all clean run

TARGETS=tcp_lat tcp_thr tcp_nodelay_thr tcp_nodelay_lat
TARGETS+=pipe_lat pipe_thr unix_lat unix_thr
TARGETS+=mempipe_lat mempipe_thr mempipe_sos22_thr
TARGETS+=vmsplice_pipe_thr vmsplice_hugepages_pipe_thr vmsplice_hugepages_coop_pipe_thr vmsplice_coop_pipe_thr
TARGETS+=shmem_pipe_thr
TARGETS+=summarise_tsc_counters

all: $(TARGETS)

%_lat: atomicio.o test.o xutil.o %_lat.o
	$(CC) -lrt $(CFLAGS) -o $@ $^

%_thr: atomicio.o test.o xutil.o %_thr.o
	$(CC) -lrt $(CFLAGS) -o $@ $^

mempipe_sos22_thr: mempipe_thr.c
	$(CC) $(CFLAGS) $^ -c -DSOS22_MEMSET -o $@

vmsplice_hugepages_pipe_thr.o: vmsplice_pipe_thr.c
	$(CC) $(CFLAGS) $^ -c -DUSE_HUGE_PAGES -o $@

vmsplice_hugepages_coop_pipe_thr.o: vmsplice_pipe_thr.c
	$(CC) $(CFLAGS) $^ -c -DUSE_HUGE_PAGES -DVMSPLICE_COOP -o $@	

vmsplice_coop_pipe_thr.o: vmsplice_pipe_thr.c
	$(CC) $(CFLAGS) $^ -c -DVMSPLICE_COOP -o $@

clean:
	rm -f *~ core *.o $(TARGETS)
