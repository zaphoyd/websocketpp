BOOST_LIB_PATH		?= /usr/local/lib
BOOST_INCLUDE_PATH ?= /usr/local/include

CFLAGS = -O2 -I$(BOOST_INCLUDE_PATH)
LDFLAGS = -L$(BOOST_LIB_PATH)

CXX		?= c++
SHARED  ?= "1"

ifeq ($(SHARED), 1)
	LDFLAGS := $(LDFLAGS) -lboost_system -lboost_thread -lboost_random -lboost_regex -lboost_date_time -lcrypto -lssl -lpthread -lwebsocketpp
else
	LDFLAGS := $(LDFLAGS) ../../libwebsocketpp.a $(BOOST_LIB_PATH)/libboost_system.a $(BOOST_LIB_PATH)/libboost_date_time.a $(BOOST_LIB_PATH)/libboost_regex.a $(BOOST_LIB_PATH)/libboost_thread.a $(BOOST_LIB_PATH)/libboost_random.a -lcrypto -lssl -lpthread
endif

stress_client: stress_client.o
	$(CXX) $(CFLAGS) $^ -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) -c $(CFLAGS) -o $@ $^

# cleanup by removing generated files
#
.PHONY:		clean
clean:
		rm -f *.o stress_client
