cxxflags=$(CXXFLAGS)
ldflags=$(LDFLAGS) -lpthread
#CXX=g++
RM=rm -f

SRCS=$(wildcard *.cpp)
OBJS=$(subst .cpp,.o,$(SRCS))

.PHONY: all depend clean exe-clean

all: server client

server: server.o
	$(CXX) $(ldflags) -o server server.o

client: client.o
	$(CXX) $(ldflags) -o client client.o

depend: .depend

.depend: $(SRCS)
	rm -f ./.depend
	$(CXX) $(cxxflags) -MM $^>>./.depend;

clean: exe-clean
	$(RM) $(OBJS)

exe-clean:
	$(RM) server client

include .depend
