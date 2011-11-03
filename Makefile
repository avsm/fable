CFLAGS = -g -Wall -O3 -D_GNU_SOURCE
LDFLAGS+=-lm

.PHONY: all clean run

#all: pipe_lat pipe_thr \
#	unix_lat unix_thr \
#	tcp_lat tcp_thr \
#	tcp_local_lat tcp_remote_lat

all: tcp_lat tcp_thr pipe_lat pipe_thr unix_lat unix_thr memflag_lat

%_lat: atomicio.o test.o xutil.o %_lat.o
	$(CC) -lrt -lm $(CFLAGS) -o $@ $^

%_thr: atomicio.o test.o xutil.o %_thr.o
	$(CC) -lrt -lm $(CFLAGS) -o $@ $^

#shm: CFLAGS += -lrt

run:
	./pipe_lat 100 10000
	./unix_lat 100 10000
	./tcp_lat 100 10000
	./pipe_thr 100 10000
	./unix_thr 100 10000
	./tcp_thr 100 10000

clean:
	rm -f *~ core
	rm -f pipe_lat pipe_thr 
	rm -f unix_lat unix_thr 
	rm -f tcp_lat tcp_thr 
	rm -f tcp_local_lat tcp_remote_lat
	rm -f shm 
