OBJS=$(patsubst %.cc,%.o,$(wildcard src/*.cc))
CCFLAGS=-Isrc -g -Wall -pthread -std=c++11

all: iVeiOTA ciVeiOTA

iVeiOTA: $(OBJS) server.cc
	g++ $(CCFLAGS) $(OBJS) $(SRCS) server.cc -o $@

ciVeiOTA: $(OBJS) client.cc
	g++ $(CCFLAGS) $(OBJS) $(SRCS) client.cc -o $@
%.o:%.cc
	g++ $(CCFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) iVeiOTA ciVeiOTA
