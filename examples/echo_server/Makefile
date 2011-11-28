BOOST_PATH = /Users/zaphoyd/Documents/boost_1_48_0/stage/lib

CFLAGS = -O2 -I/Users/zaphoyd/Documents/boost_1_48_0
LDFLAGS = -L/Users/zaphoyd/Documents/boost_1_48_0/stage/lib

CXX		?= c++
SHARED  ?= "1"

ifeq ($(SHARED), 1)
	LDFLAGS := $(LDFLAGS) -lboost_system -lboost_date_time -lboost_program_options -lwebsocketpp
else
	LDFLAGS := $(LDFLAGS) $(BOOST_PATH)/libboost_system.a $(BOOST_PATH)/libboost_date_time.a $(BOOST_PATH)/libboost_regex.a -lcrypto -lssl ../../libwebsocketpp.a
endif

echo_server: echo_server.o
	$(CXX) $(CFLAGS) $^ -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) -c $(CFLAGS) -o $@ $^

# cleanup by removing generated files
#
.PHONY:		clean
clean:
		rm -f *.o echo_server
