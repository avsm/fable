all_target := posix
uname := $(shell uname -s)
ifeq ($(uname),Linux)
all_target := linux
endif
ifeq ($(uname),OpenBSD)
all_target := openbsd
endif

.PHONY: all clean run

CFLAGS := -g -Wall -O3 -D_GNU_SOURCE -DNDEBUG -std=gnu99

LDFLAGS_Linux := -lrt -lnuma
LDFLAGS += -lm $(LDFLAGS_$(uname))

TARGETS_POSIX := pipe_thr tcp_thr tcp_nodelay_thr unix_thr mempipe_spin_thr
TARGETS_Linux += mempipe_thr vmsplice_pipe_thr vmsplice_hugepages_pipe_thr vmsplice_hugepages_coop_pipe_thr vmsplice_coop_pipe_thr

TARGETS_POSIX += pipe_lat unix_lat tcp_lat tcp_nodelay_lat mempipe_lat
TARGETS_Linux += shmem_pipe_thr futex_lat

TARGETS_POSIX += summarise_tsc_counters

TARGETS_OpenBSD := 

TARGETS := $(TARGETS_POSIX) $(TARGETS_$(uname))

x-%:
	echo $($*)

all: $(TARGETS)
	@ :

%_lat: atomicio.o test.o xutil.o %_lat.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $^

%_thr: atomicio.o test.o xutil.o %_thr.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $^

tcp_nodelay_thr.o: tcp_thr.c
	$(CC) $(CFLAGS) $^ -c -DUSE_NODELAY -o $@

tcp_nodelay_lat.o: tcp_lat.c
	$(CC) $(CFLAGS) $^ -c -DUSE_NODELAY -o $@

mempipe_spin_thr.o: mempipe_thr.c
	$(CC) $(CFLAGS) $^ -c -DNO_FUTEX -o $@

vmsplice_hugepages_pipe_thr.o: vmsplice_pipe_thr.c
	$(CC) $(CFLAGS) $^ -c -DUSE_HUGE_PAGES -o $@

vmsplice_hugepages_coop_pipe_thr.o: vmsplice_pipe_thr.c
	$(CC) $(CFLAGS) $^ -c -DUSE_HUGE_PAGES -DVMSPLICE_COOP -o $@	

vmsplice_coop_pipe_thr.o: vmsplice_pipe_thr.c
	$(CC) $(CFLAGS) $^ -c -DVMSPLICE_COOP -o $@

clean:
	rm -f *~ core *.o $(TARGETS)

