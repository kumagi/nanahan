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

const int thread_max = 2;
const int thread_tries = 10000;
const int all_tries = 100;

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
  const int thread_id = w->i_am;
  unsigned int seed = w->seed_;
  const int tries = w->tries_;
  delete w;

  s.prepare();
  pthread_barrier_wait(&barrier);

  for(int i=0; i < tries; ++i){
    random_sleep(&seed);
    s.push(thread_id + thread_max*i);
  }
  random_sleep(&seed);
  for(int i=0; i < tries; ++i){
    random_sleep(&seed);
    int result = s.pop();
  }
  return NULL;
}

bool invariant_check(const stack& s) {
  //if(s.size() != thread_tries*thread_max){
  if(s.size() != 0){
    cout << s << endl;
    std::cout << " size max actual: " << s.size()
              << " expected " << 0 << std::endl;
    return false;
  }
  return true;
}

int main(int argc, char* argv[]){
  pthread_barrier_init(&barrier, NULL, thread_max);
  std::cout << "thread:" << thread_max << std::endl
            << "each:" << thread_tries << std::endl
            << "try:" << all_tries << std::endl;
  for(unsigned int j = 0; j < all_tries; ++j) {
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
    std::cout << "finish" << std::endl;
    if(!invariant_check(s_)){
      std::cout << s_ << std::endl;
      return 1;
    }
  }
  return 0;
}
