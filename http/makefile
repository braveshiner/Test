CURR_PATH=$(shell pwd)
src=http.c
bin=http
cc=gcc
LDFLAGS=-lpthread

$(bin):$(src)
	$(cc)  -o  $@  $^  $(LDFLAGS)
.PHONY:clean
clean:
	rm -f $(bin)

