CFLAGS = -O2
LDFLAGS = 

CXX		?= c++
SHARED  ?= "1"

ifeq ($(SHARED), 1)
	LDFLAGS := $(LDFLAGS) -lboost_system -lboost_thread -lwebsocketpp
else
	LDFLAGS := $(LDFLAGS) -lboost_system -lboost_thread -lboost_date_time -lboost_regex -lboost_random -lboost_program_options ../../libwebsocketpp.a
endif

echo_client: echo_client.o echo_client_handler.o
	$(CXX) $(CFLAGS) $^ -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) -c $(CFLAGS) -o $@ $^

# cleanup by removing generated files
#
.PHONY:		clean
clean:
		rm -f *.o echo_client
