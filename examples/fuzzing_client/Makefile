BOOST_LIB_PATH		?= /usr/local/lib
BOOST_INCLUDE_PATH ?= /usr/local/include
WEBSOCKETPP_PATH    ?= ../../
CPP11               ?= 

CFLAGS = -Wall -O2 $(CPP11) -I$(BOOST_INCLUDE_PATH)
LDFLAGS = -L$(BOOST_LIB_PATH)

CXX		?= c++
SHARED  ?= "1"

ifeq ($(SHARED), 1)
	LDFLAGS := $(LDFLAGS) -lwebsocketpp -lpthread -lboost_system -lboost_thread -lboost_random -lboost_regex - lboost_date_time
else
	LDFLAGS := $(LDFLAGS) $(WEBSOCKETPP_PATH)/libwebsocketpp.a $(BOOST_LIB_PATH)/libboost_system.a $(BOOST_LIB_PATH)/libboost_date_time.a $(BOOST_LIB_PATH)/libboost_regex.a $(BOOST_LIB_PATH)/libboost_thread.a $(BOOST_LIB_PATH)/libboost_random.a -lpthread
endif

fuzzing_client: fuzzing_client.o
	$(CXX) $(CFLAGS) $^ -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) -c $(CFLAGS) -o $@ $^

# cleanup by removing generated files
#
.PHONY:		clean
clean:
		rm -f *.o fuzzing_client
