OBJS=$(patsubst %.cc,%.o,$(wildcard src/*.cc))
CCFLAGS=-Isrc -g -Wall -pthread -std=c++11

all: iv_v4_hal

iv_v4_hal: $(OBJS) server.cc
	g++ $(CCFLAGS) $(OBJS) $(SRCS) server.cc -o $@

%.o:%.cc
	g++ $(CCFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) iVeiOTA ciVeiOTA
