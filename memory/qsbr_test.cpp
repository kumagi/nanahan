#include "qsbr.hpp"
#include <pthread.h>
#include <vector>
#define mb() asm volatile("" : : : "memory")

struct hoge{
  uint64_t foo;
  uint64_t bar;
  hoge(uint64_t a, uint64_t b) :foo(a), bar(b) {};
  ~hoge(){ }
};

const int threads = 128;

struct workingset {
  std::vector<hoge*>& hoges_;
  int index_;
  qsbr* q_;
  int num_;
  pthread_barrier_t* barrier_;
  workingset(std::vector<hoge*>& hoge,
             int i,
             qsbr* q,
             int num,
             pthread_barrier_t* barrier)
    :hoges_(hoge), index_(i), q_(q), num_(num), barrier_(barrier) {}
};

void* running(void* target) {
  workingset* w = reinterpret_cast<workingset*>(target);
  std::vector<hoge*>& hoges = w->hoges_;
  const int my_index = w->index_;
  const int num = w->num_;
  pthread_barrier_wait(w->barrier_);

  usleep(rand() % 10000);
  {
    qsbr::ref_guard g(*w->q_);
    uint64_t sum = 0;
    for (int i=w->num_; 0 <= i; --i) {
      const int index = (my_index + i) % hoges.size();
      if (hoges[index]) {
        mb();
        const uint64_t num = hoges[index]->foo;
        mb();
        if (hoges[index]) {
          sum += num;
        }
      }
      usleep(1);
    }
    hoge* delete_target = w->hoges_[w->index_];
    w->hoges_[w->index_] = NULL;
    w->q_->safe_free(delete_target);
    std::cout << "d" << std::flush;
  }
  delete w;
}

int main() {
  qsbr q;
  pthread_barrier_t barrier;
  pthread_barrier_init(&barrier, 0, threads);
  pthread_t worker[threads];

  std::vector<hoge*> hoges;
  for (int i = 0; i < threads; ++i) {
    hoges.emplace_back(new hoge(i*100, i*i*200));
  }
  for (int i = 0; i < threads; ++i) {
    pthread_create(&worker[i],
                   NULL,
                   running,
                   new workingset(hoges, i, &q, (rand() % 1024), &barrier));
  }
  for (int i = 0; i < threads; ++i) {
    pthread_join( worker[i], NULL);
  }
  pthread_barrier_destroy(&barrier);
  return 0;
}
