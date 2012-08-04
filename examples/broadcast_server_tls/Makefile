BOOST_LIBS=boost_system boost_date_time boost_regex boost_thread

include ../common.mk

LDFLAGS := $(LDFLAGS) -lcrypto -lssl -lpthread

broadcast_server: broadcast_server_tls.o
	$(CXX) $(CFLAGS) $^ -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) -c $(CFLAGS) -o $@ $^

# cleanup by removing generated files
#
.PHONY:		clean
clean:
		rm -f *.o broadcast_server
