BOOST_LIBS=boost_system boost_date_time boost_thread boost_regex

include ../common.mk

LDFLAGS := $(LDFLAGS) -lcrypto -lssl -lpthread

echo_server_tls: echo_server_tls.o
	$(CXX) $(CFLAGS) $^ -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) -c $(CFLAGS) -o $@ $^

# cleanup by removing generated files
#
.PHONY:		clean
clean:
		rm -f *.o echo_server_tls
