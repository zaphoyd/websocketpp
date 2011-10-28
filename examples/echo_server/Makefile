CFLAGS = -O2
LDFLAGS = 

CXX		?= c++
SHARED  ?= "1"

ifeq ($(SHARED), 1)
	LDFLAGS := $(LDFLAGS) -lboost_system -lboost_date_time -lwebsocketpp
else
	LDFLAGS := $(LDFLAGS) -lboost_system -lboost_date_time -lboost_regex -lboost_random -lboost_program_options ../../libwebsocketpp.a
endif

echo_server: echo_server.o echo.o
	$(CXX) $(CFLAGS) $^ -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) -c $(CFLAGS) -o $@ $^

# cleanup by removing generated files
#
.PHONY:		clean
clean:
		rm -f *.o echo_server
