#
#  Makefile
#  
#  Copyright (C) 1998, 2009
#  Paul E. Jones <paulej@packetizer.com>
#  All Rights Reserved.
#
#############################################################################
#  $Id: Makefile 12 2009-06-22 19:34:25Z paulej $
#############################################################################
#
#  Description:
#	This is a makefile for UNIX to build the programs sha, shacmp, and
#	shatest
#
#

CC	= g++

CFLAGS	= -c -O2 -Wall -D_FILE_OFFSET_BITS=64

LIBS	=

OBJS	= sha1.o

all: sha shacmp shatest

sha: sha.o $(OBJS)
	$(CC) -o $@ sha.o $(OBJS) $(LIBS)

shacmp: shacmp.o $(OBJS)
	$(CC) -o $@ shacmp.o $(OBJS) $(LIBS)

shatest: shatest.o $(OBJS)
	$(CC) -o $@ shatest.o $(OBJS) $(LIBS)

%.o: %.cpp
	$(CC) $(CFLAGS) -o $@ $<

clean:
	$(RM) *.o sha shacmp shatest
