CXX= g++
OPTS= -O0 -g -std=gnu++0x
PROF=
#-pg -fno-inline
WARNS=-W -Wall -Wextra -Wformat=2 -Wstrict-aliasing=4 -Wcast-qual -Wcast-align \
	-Wwrite-strings -Wfloat-equal -Wpointer-arith -Wswitch-enum
TEST_LD= -lpthread -lboost_thread
GTEST_INC= -I$(GTEST_DIR)/include -I$(GTEST_DIR)
GTEST_DIR=/opt/google/gtest-1.6.0
COMMON=-I../include

NOTIFY=&& notify-send Test success! -i ~/themes/ok_icon.png || notify-send Test failed... -i ~/themes/ng_icon.png

target:test

test_map:test_map.o map.o libgtest.a
	$(CXX) $^ -o $@ $(OPTS) $(PROF) $(TEST_LD) $(WARNS)
test:test_map
	./test_map --gtest_color=no $(NOTIFY)
%.o:%.cc map.hpp
	$(CXX) -c $< -o $@ $(OPTS) $(PROF) $(GTEST_INC) $(HASHMAP_OPS) $(WARNS)

# gtest
libgtest.a:
	$(CXX) ${GTEST_INC} -c ${GTEST_DIR}/src/gtest-all.cc -o gtest-all.o
	$(CXX) ${GTEST_INC} -c ${GTEST_DIR}/src/gtest_main.cc -o gtest_main.o
	ar -rv libgtest.a gtest-all.o gtest_main.o

gtest_main.o:
	$(CXX) $(GTEST_INC) -c $(OPTS) $(GTEST_DIR)/src/gtest_main.cc -o $@
gtest-all.o:
	$(CXX) $(GTEST_INC) -c $(OPTS) $(GTEST_DIR)/src/gtest-all.cc -o $@
gtest_main.a:gtest-all.o gtest_main.o
	ar -r $@ $^

clean:
	rm -f *.o
	rm -f *~
