CFLAGS = -O2
LDFLAGS = 

CXX		?= c++
SHARED  ?= "1"

ifeq ($(SHARED), 1)
	LDFLAGS := $(LDFLAGS) -lboost_system -lboost_thread -lwebsocketpp
else
	LDFLAGS := $(LDFLAGS) -lboost_system -lboost_thread -lboost_date_time -lboost_regex -lboost_random -lboost_program_options ../../libwebsocketpp.a
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
