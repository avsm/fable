CFLAGS = -g -Wall -O3 -D_GNU_SOURCE
LDFLAGS+=-lm

.PHONY: all clean run

TARGETS=tcp_lat tcp_thr tcp_nodelay_thr tcp_nodelay_lat pipe_lat pipe_thr unix_lat unix_thr mempipe_lat mempipe_thr mempipe_thr_sos22 vmsplice_pipe_thr summarise_tsc_counters shmem_pipe_thr

all: $(TARGETS)

%_lat: atomicio.o test.o xutil.o %_lat.o
	$(CC) -lrt $(CFLAGS) -o $@ $^

%_thr: atomicio.o test.o xutil.o %_thr.o
	$(CC) -lrt $(CFLAGS) -o $@ $^

mempipe_thr_sos22.o: mempipe_thr.c
	$(CC) $(CFLAGS) mempipe_thr.c -c -DSOS22_MEMSET -o mempipe_thr_sos22.o

mempipe_thr_sos22: atomicio.o test.o xutil.o mempipe_thr_sos22.o
	$(CC) -lrt $(CFLAGS) -o $@ $^

clean:
	rm -f *~ core $(TARGETS)
	rm -f mempipe_thr_sos22.o
