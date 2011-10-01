CFLAGS = -O2
LDFLAGS = 

CXX		?= c++
SHARED  ?= "1"

ifeq ($(SHARED), 1)
	LDFLAGS := $(LDFLAGS) -lboost_system -lboost_thread -lwebsocketpp
else
	LDFLAGS := $(LDFLAGS) -lboost_system -lboost_thread -lboost_date_time -lboost_regex -lboost_random ../../libwebsocketpp.a
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
