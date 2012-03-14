################################################################################
# 
#  Copyright (c) 2011, Peter Thorson. All rights reserved.
# 
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are met:
#      * Redistributions of source code must retain the above copyright
#        notice, this list of conditions and the following disclaimer.
#      * Redistributions in binary form must reproduce the above copyright
#        notice, this list of conditions and the following disclaimer in the
#        documentation and/or other materials provided with the distribution.
#      * Neither the name of the WebSocket++ Project nor the
#        names of its contributors may be used to endorse or promote products
#        derived from this software without specific prior written permission.
#  
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
#  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
#  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
#  ARE DISCLAIMED. IN NO EVENT SHALL PETER THORSON BE LIABLE FOR ANY
#  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
#  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
#  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
#  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
#  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#  
#  This Makefile was derived from a similar one included in the libjson project
#  It's authors were Jonathan Wallace and Bernhard Fluehmann.


objects = websocket_server_session.o websocket_client_session.o websocket_session.o  websocket_server.o websocket_client.o websocket_frame.o \
          network_utilities.o sha1.o base64.o

libs = -lboost_system -lboost_date_time -lboost_regex -lboost_random

OS=$(shell uname)

# Defaults
ifeq ($(OS), Darwin)
	cxxflags_default = -c -O2 -DNDEBUG
else
	cxxflags_default = -c -O2 -DNDEBUG
endif
cxxflags_small   = -c 
cxxflags_debug   = -c -g
cxxflags_shared  = -f$(PIC)
libname          = libwebsocketpp
libname_hdr      = websocketpp
libname_debug    = $(libname)
suffix_shared    = so
suffix_shared_darwin = dylib
suffix_static    = a
major_version    = 0
minor_version    = 1.0
objdir           = objs

# Variables
prefix          ?= /usr/local
exec_prefix     ?= $(prefix)
libdir          ?= lib
includedir      ?= include
srcdir          ?= src
CXX             ?= c++
AR              ?= ar
PIC             ?= PIC
BUILD_TYPE      ?= default
SHARED          ?= 1

ifneq ($(OS),Darwin)
	ldconfig = ldconfig
else
	ldconfig = 
endif

# Internal Variables
inst_path        = $(exec_prefix)/$(libdir)
include_path     = $(prefix)/$(includedir)

# BUILD_TYPE specific settings
ifeq ($(BUILD_TYPE), debug)
	CXXFLAGS     := $(cxxflags_debug) $(CXXFLAGS_EXTRA)
	libname     := $(libname_debug)
else
	CXXFLAGS    := $(cxxflags_default) $(CXXFLAGS_EXTRA)
endif

# SHARED specific settings
ifeq ($(SHARED), 1)
	ifeq ($(OS), Darwin)
		libname_shared           = $(libname).$(suffix_shared_darwin)
	else
		libname_shared           = $(libname).$(suffix_shared)
	endif
	libname_shared_major_version = $(libname_shared).$(major_version)
	lib_target                   = $(libname_shared_major_version).$(minor_version)
	objdir                      := $(objdir)_shared
	CXXFLAGS                    := $(CXXFLAGS) $(cxxflags_shared)
else
	lib_target                   = $(libname).$(suffix_static)
	objdir                      := $(objdir)_static
endif

# Phony targets
.PHONY: all banner installdirs install install_headers clean uninstall \
        uninstall_headers

# Targets
all: banner $(lib_target)
	@echo "============================================================"
	@echo "Done"
	@echo "============================================================"

banner:
	@echo "============================================================"
	@echo "libwebsocketpp version: "$(major_version).$(minor_version) "target: "$(target) "OS: "$(OS)
	@echo "============================================================"

installdirs: banner
	mkdir -p $(objdir)

# Libraries
ifeq ($(SHARED),1)
$(lib_target): banner installdirs $(addprefix $(objdir)/, $(objects))
	@echo "Link "
	cd $(objdir) ; \
	if test "$(OS)" = "Darwin" ; then \
		$(CXX) -dynamiclib $(libs) $(LDFLAGS) -Wl,-dylib_install_name -Wl,$(libname_shared_major_version) -o $@ $(objects) ; \
	else \
		$(CXX) -shared $(libs) $(LDFLAGS) -Wl,-soname,$(libname_shared_major_version) -o $@ $(objects) ; \
	fi ; \
	mv -f $@ ../
	@echo "Link: Done"
else
$(lib_target): banner installdirs $(addprefix $(objdir)/, $(objects))
	@echo "Archive"
	cd $(objdir) ; \
	$(AR) -cvq $@ $(objects) ; \
	mv -f $@ ../
	@echo "Archive: Done"
endif

# Compile object files
$(objdir)/sha1.o: $(srcdir)/sha1/sha1.cpp installdirs
	$(CXX) $< -o $@ $(CXXFLAGS)
	
$(objdir)/base64.o: $(srcdir)/base64/base64.cpp installdirs
	$(CXX) $< -o $@ $(CXXFLAGS)

$(objdir)/%.o: $(srcdir)/%.cpp installdirs
	$(CXX) $< -o $@ $(CXXFLAGS)

ifeq ($(SHARED),1)
install: banner install_headers $(lib_target)
	@echo "Install shared library"
	cp -f ./$(lib_target) $(inst_path)
	cd $(inst_path) ; \
	ln -sf $(lib_target) $(libname_shared_major_version) ; \
	ln -sf $(libname_shared_major_version) $(libname_shared)
	$(ldconfig)
	@echo "Install shared library: Done."
else
install: banner install_headers $(lib_target)
	@echo "Install static library"
	cp -f ./$(lib_target) $(inst_path)
	@echo "Install static library: Done."
endif

install_headers: banner
	@echo "Install header files"
	mkdir -p $(include_path)/$(libname_hdr)
#	cp -f ./*.hpp $(include_path)/$(libname_hdr)
	cp -f ./$(srcdir)/*.hpp $(include_path)/$(libname_hdr)
	mkdir -p $(include_path)/$(libname_hdr)/base64
	cp -f ./$(srcdir)/base64/base64.h $(include_path)/$(libname_hdr)/base64
	mkdir -p $(include_path)/$(libname_hdr)/sha1
	cp -f ./$(srcdir)/sha1/sha1.h $(include_path)/$(libname_hdr)/sha1
	chmod -R a+r $(include_path)/$(libname_hdr)
	find  $(include_path)/$(libname_hdr) -type d -exec chmod a+x {} \;
	@echo "Install header files: Done."

clean: banner
	@echo "Clean library and object folder"
	rm -rf $(objdir)
	rm -f $(lib_target)
	@echo "Clean library and object folder: Done"

ifeq ($(SHARED),1)
uninstall: banner uninstall_headers
	@echo "Uninstall shared library"
	rm -f $(inst_path)/$(libname_shared)
	rm -f $(inst_path)/$(libname_shared_major_version)
	rm -f $(inst_path)/$(lib_target)
	ldconfig
	@echo "Uninstall shared library: Done"
else
uninstall: banner uninstall_headers
	@echo "Uninstall static library"
	rm -f $(inst_path)/$(lib_target)
	@echo "Uninstall static library: Done"
endif

uninstall_headers: banner
	@echo "Uninstall header files"
	rm -rf $(include_path)/$(libname)
	@echo "Uninstall header files: Done"
