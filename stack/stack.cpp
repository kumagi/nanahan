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

const int thread_max = 20;
const int thread_tries = 10;

const int all_tries = 1;

pthread_barrier_t barrier;
pthread_mutex_t mp_lock;

//using namespace std;
using std::cout;
using std::endl;

typedef nanahan::Stack<int> stack;

inline void random_sleep(unsigned int* seed){
  usleep(((rand_r(seed) % 100) + 1)*10);
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

  pthread_mutex_lock(&mp_lock);
  std::stringstream ss;
  ss << "[" << "i" << s.get_index() << "]";
  std::cout << ss.str();
  pthread_mutex_unlock(&mp_lock);
  pthread_barrier_wait(&barrier);


  try {
    for(int i=0; i < tries; ++i) {
      random_sleep(&seed);
      s.push(thread_id * tries + i);
    }
    random_sleep(&seed);
    for(int i=0; i < tries; ++i) {
      random_sleep(&seed);
      int result = s.pop();
      std::cerr << " " << result << std::flush;
    }
  } catch (...) {
    std::cout << "invalid!!!" << std::endl;
    usleep(50000);
    exit(1);
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
  pthread_mutex_init(&mp_lock, NULL);
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
    std::cout << "finish. checking..";
    if(!invariant_check(s_)){
      return 1;
    }
    std::cout <<"stack empty." << std::endl;
  }

  std::cout << "all finish" << std::endl;
  return 0;
}
