BOOST_LIBS=boost_system boost_thread boost_regex boost_random

include ../common.mk

LDFLAGS := $(LDFLAGS) -lpthread

chat_client: chat_client.o chat_client_handler.o
	$(CXX) $(CFLAGS) $^ -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) -c $(CFLAGS) -o $@ $^

# cleanup by removing generated files
#
.PHONY:		clean
clean:
		rm -f *.o chat_client
