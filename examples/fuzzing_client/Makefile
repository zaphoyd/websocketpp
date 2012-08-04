BOOST_LIBS=boost_system boost_date_time boost_thread boost_regex boost_random

include ../common.mk

LDFLAGS := $(LDFLAGS) -lpthread

fuzzing_client: fuzzing_client.o
	$(CXX) $(CFLAGS) $^ -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) -c $(CFLAGS) -o $@ $^

# cleanup by removing generated files
#
.PHONY:		clean
clean:
		rm -f *.o fuzzing_client
