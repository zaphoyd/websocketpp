BOOST_LIB_PATH		?= /usr/local/lib
BOOST_INCLUDE_PATH ?= /usr/local/include
WEBSOCKETPP_PATH    ?= ../../
CPP11               ?= 

CFLAGS = -Wall -O2 $(CPP11) -I$(BOOST_INCLUDE_PATH)
LDFLAGS = -L$(BOOST_LIB_PATH)

CXX		?= c++
SHARED  ?= "1"

OS=$(shell uname)

ifeq ($(OS), Darwin)
	darwinlibs = 
else
	darwinlibs = -lrt -lpthread
endif

ifeq ($(SHARED), 1)
	LDFLAGS := $(LDFLAGS) -lwebsocketpp $(darwinlibs) -lboost_system -lboost_thread -lboost_random -lboost_regex -lboost_date_time -boost_chrono
else
	LDFLAGS := $(LDFLAGS) $(WEBSOCKETPP_PATH)/libwebsocketpp.a $(BOOST_LIB_PATH)/libboost_system.a $(BOOST_LIB_PATH)/libboost_date_time.a $(BOOST_LIB_PATH)/libboost_regex.a $(BOOST_LIB_PATH)/libboost_thread.a $(BOOST_LIB_PATH)/libboost_random.a $(BOOST_LIB_PATH)/libboost_chrono.a $(darwinlibs)
endif

wsperf: wsperf.o
	$(CXX) $(CFLAGS) $^ -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) -c $(CFLAGS) -o $@ $^

# cleanup by removing generated files
#
.PHONY:		clean
clean:
		rm -f *.o wsperf
