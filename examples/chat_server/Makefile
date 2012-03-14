BOOST_LIBS=boost_system boost_date_time boost_program_options boost_thread boost_regex

include ../common.mk

LDFLAGS := $(LDFLAGS) -lpthread

chat_server: chat_server.o chat.o
	$(CXX) $(CFLAGS) $^ -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) -c $(CFLAGS) -o $@ $^

# cleanup by removing generated files
#
.PHONY:		clean
clean:
		rm -f *.o chat_server
