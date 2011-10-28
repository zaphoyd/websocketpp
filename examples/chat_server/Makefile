CFLAGS = -O2
LDFLAGS = 

CXX		?= c++
SHARED  ?= "1"

ifeq ($(SHARED), 1)
	LDFLAGS := $(LDFLAGS) -lboost_system -lboost_date_time -lwebsocketpp
else
	LDFLAGS := $(LDFLAGS) -lboost_system -lboost_date_time -lboost_regex -lboost_random  -lboost_program_options ../../libwebsocketpp.a
endif

chat_server: chat_server.o chat.o
	$(CXX) $(CFLAGS) $^ -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) -c $(CFLAGS) -o $@ $^

# cleanup by removing generated files
#
.PHONY:		clean
clean:
		rm -f *.o chat_server
