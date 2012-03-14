CFLAGS = -O2
LDFLAGS = 

CXX		?= c++
SHARED  ?= "1"

ifeq ($(SHARED), 1)
	LDFLAGS := $(LDFLAGS) -lboost_system -lboost_thread -lwebsocketpp
else
	LDFLAGS := $(LDFLAGS) ../../libwebsocketpp.a $(BOOST_LIB_PATH)/libboost_system.a $(BOOST_LIB_PATH)/libboost_date_time.a $(BOOST_LIB_PATH)/libboost_regex.a $(BOOST_LIB_PATH)/libboost_random.a $(BOOST_LIB_PATH)/libboost_thread.a -lpthread
endif

chat_client: chat_client.o chat_client_handler.o
	$(CXX) $(CFLAGS) $^ -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) -c $(CFLAGS) -o $@ $^

# cleanup by removing generated files
#
.PHONY:		clean
clean:
		rm -f *.o chat_client
