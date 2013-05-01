CXX=g++
CXXFLAGS=-ggdb -O0 -std=gnu++0x -march=x86-64 -fno-inline
LDFLAGS=-lpthread -lstdc++

target:test
test:stack
	./stack
stack:stack.o
stack.o: stack.cpp stack.hpp ../memory/qsbr.hpp

