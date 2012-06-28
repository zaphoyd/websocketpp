BOOST_LIBS=boost_system boost_date_time boost_regex boost_thread boost_random boost_chrono boost_program_options

include ../common.mk

LDFLAGS := $(LDFLAGS) -lcrypto -lssl -lpthread
CFLAGS := -Wall -O3 $(CFLAGS)

OS=$(shell uname)

ifneq ($(OS), Darwin)
	LDFLAGS := $(LDFLAGS) -lrt -lpthread
endif

wsperf: wsperf.o request.o case.o generic.o wscmd.o stress_aggregate.o stress_handler.o
	$(CXX) $(CFLAGS) $^ -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) -c $(CFLAGS) -o $@ $^

# cleanup by removing generated files
#
.PHONY:		clean
clean:
		rm -f *.o wsperf wsperf_client
