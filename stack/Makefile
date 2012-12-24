CXX=g++-4.5
CXXFLAGS=-g -O0 -Wno-deprecated -std=gnu++0x -march=x86-64
LDFLAGS=-lpthread -lglog

target:test
test:stack
	GLOG_logtostderr=1 ./stack
stack:stack.o
stack.o: stack.cpp stack.hpp

