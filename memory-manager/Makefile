cxxflags=$(CXXFLAGS) -std=c++11 \
		-I../src/od-fs -I../src/od-fs/include \
		-I../src/include -I../gensrc -I../src \
		-I../$(libfsemu_dir)/include \
		-I$(libfsemu_dir)/src/lua
ldflags=$(LDFLAGS) -lpthread -lcrypto
#CXX=g++
RM=rm -f

SRCS=$(wildcard *.cpp)
OBJS=$(subst .cpp,.o,$(SRCS))

.PHONY: all depend clean exe-clean .depend

all: server client.o

server: server.o MemoryStorage.o
	$(CXX) $(ldflags) -o server $^
	cp server ..

MemoryStorage: MemoryStorage.o
	$(CXX) $(ldflags) -o $@ $^

server.o: server.cpp
	$(CXX) $(cxxflags) -c -o server.o server.cpp
	
client.o: client.cpp
	$(CXX) $(cxxflags) -c -o client.o client.cpp

MemoryStorage.o: MemoryStorage.cpp
	$(CXX) $(cxxflags) -c -o MemoryStorage.o MemoryStorage.cpp

#depend: .depend
#
#.depend: $(SRCS)
#	rm -f ./.depend
#	$(CXX) $(cxxflags) -MM $^>>./.depend;

clean: exe-clean
	$(RM) $(OBJS)

exe-clean:
	$(RM) server client mem_socket

include .depend
