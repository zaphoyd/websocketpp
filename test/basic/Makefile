CFLAGS = -O2
LDFLAGS = 

CXX		?= c++
SHARED  ?= "1"

ifeq ($(SHARED), 1)
	LDFLAGS := $(LDFLAGS) -lboost_system -lboost_date_time -lboost_regex -lboost_unit_test_framework -lwebsocketpp
else
	LDFLAGS := $(LDFLAGS) -lboost_system -lboost_date_time -lboost_regex -lboost_unit_test_framework ../../libwebsocketpp.a
endif

tests: parsing.cpp 
	$(CXX) $(CFLAGS) $^ -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) -c $(CFLAGS) -o $@ $^

# cleanup by removing generated files
#
.PHONY:		clean
clean:
		rm -f *.o tests
