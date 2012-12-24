/* -*- Mode: C; tab-width: 2; c-basic-offset: 2; indent-tabs-mode: nil; compile-command: "make -j2 test" -*- */
#include "stack.hpp"
#include <iostream>
#include <sstream>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>

//(setq compile-command "make -j2 test")

//(setq c-basic-offset 2)
const int thread_max = 3;
const int thread_tries = 100;
pthread_barrier_t barrier;
//using namespace std;
using std::cout;
using std::endl;

typedef nanahan::Stack<int> stack;

inline void random_sleep(unsigned int* seed){
  usleep(((rand_r(seed) % 10) + 1)*10);
}

struct workingset {
  stack* s_;
  int i_am;
  unsigned int seed_;
  int tries_;
  workingset(stack* s, int i, unsigned int seed, int tries)
  :s_(s), i_am(i), seed_(seed), tries_(tries){}
};

void init() {
}

void* work(void* ptr) {
  workingset* w = reinterpret_cast<workingset*>(ptr);
  stack& s = *w->s_;
  int thread_id = w->i_am;
  unsigned int seed = w->seed_;
  int tries = w->tries_;
  delete w;
  s.prepare();
  pthread_barrier_wait(&barrier);

  for(int i=0; i < tries; ++i){
    random_sleep(&seed);
    s.push(thread_id + thread_max*i);
  }
  return NULL;
}

bool invariant_check(stack& s) {
  if(s.size() != thread_tries*thread_max){
    cout << s << endl;
    std::cout << " size max actual: " << s.size()
              << " expected " <<thread_tries*thread_max << std::endl;
    return false;
  }
  return true;
}

int main(int argc, char* argv[]){
  pthread_barrier_init(&barrier, NULL, thread_max);
  int tries = 1000;
  std::cout << "thread:" << thread_max << std::endl
            << "each:" << thread_tries << std::endl
            << "try:" << tries << std::endl;
  for(unsigned int j = 0; j < 100; ++j) {
    std::stringstream ss;

    stack s_(thread_max, ss);
    pthread_t th[thread_max];
    for (int i = 0; i < thread_max; ++i){
      workingset* w = new workingset(&s_, i, j*i, thread_tries);
      pthread_create(&th[i], NULL, work, (void*)w);
    }
    for (int i = 0; i < thread_max; ++i){
      void* result;
      pthread_join(th[i], &result);
    }
    if(!invariant_check(s_)){
      std::cout << ss.str() << std::endl;
      return 1;
    }
  }
  return 0;
}
