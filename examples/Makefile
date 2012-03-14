DIRS=broadcast_server_tls chat_client chat_server concurrent_server echo_client echo_server echo_server_tls fuzzing_client fuzzing_server_tls stress_client wsperf

.PHONY:
all: build

.PHONY:
build: $(DIRS:%=%.build)

.PHONY:
clean: $(DIRS:%=%.clean)

.PHONY:
%.build:
	$(MAKE) -C $(@:%.build=%)

.PHONY:
%.clean:
	$(MAKE) -C $(@:%.clean=%) clean
