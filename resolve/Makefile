ARCHS=48native 48xennopin 48xenpin
SIZE=128
TESTS=pipe_lat tcp_lat unix_lat mempipe_lat

find_tests = $(foreach test,$(TESTS),$(arch)-$(test).csv)
allt := $(foreach arch,$(ARCHS),$(find_tests))

all: $(allt)
	@ :

%.csv:
	export ARCH=$(shell echo $(*F) | awk -F - '{print $$1}'); \
	export TEST=$(shell echo $(*F) | awk -F - '{print $$2}'); \
	export SIZE=$(SIZE); \
	./lat.sh

clean:
	rm -f *.csv
