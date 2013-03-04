CXX=g++
CXXFLAGS=-g -O0 -Wno-deprecated -std=gnu++0x -march=x86-64 -fno-inline
LDFLAGS=-lpthread -lstdc++

target:test
test:stack
	./stack
stack:stack.o
stack.o: stack.cpp stack.hpp

