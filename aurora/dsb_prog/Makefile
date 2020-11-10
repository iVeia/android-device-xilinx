TARGET := pdsb

SRCS :=

CC := g++
CPPFLAGS := -g -Wall -std=c++17

SRCS := prog.cc dsb.cc util.cc
OBJS := $(SRCS:.cc=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CPPFLAGS) $(OBJS) -o $@

%.o : %.cc
	$(CC) $(CPPFLAGS) -c $<

clean:
	rm -f $(OBJS)

distclean: clean
	rm -f $(TARGET)

