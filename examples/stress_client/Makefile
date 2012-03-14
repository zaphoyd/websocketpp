BOOST_LIBS=boost_system boost_date_time boost_thread boost_random boost_regex

include ../common.mk

LDFLAGS := $(LDFLAGS) -lcrypto -lssl -lpthread

stress_client: stress_client.o
	$(CXX) $(CFLAGS) $^ -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) -c $(CFLAGS) -o $@ $^

# cleanup by removing generated files
#
.PHONY:		clean
clean:
		rm -f *.o stress_client
