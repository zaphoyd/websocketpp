BOOST_PREFIX ?= /usr/local
BOOST_LIB_PATH		?= $(BOOST_PREFIX)/lib
BOOST_INCLUDE_PATH ?= $(BOOST_PREFIX)/include

CPP11               ?= 

CFLAGS = -Wall -O2 $(CPP11) -I$(BOOST_INCLUDE_PATH)
LDFLAGS = -L$(BOOST_LIB_PATH)

CXX		?= c++
SHARED  ?= 0

ifeq ($(SHARED), 1)
	LDFLAGS := $(LDFLAGS) -lwebsocketpp
	LDFLAGS := $(LDFLAGS) $(BOOST_LIBS:%=-l%)
else
	LDFLAGS := $(LDFLAGS) ../../libwebsocketpp.a
	LDFLAGS := $(LDFLAGS) $(BOOST_LIBS:%=$(BOOST_LIB_PATH)/lib%.a)
endif
