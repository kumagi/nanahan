#ifndef CRISP_H
#define CRISP_H
#include <cstddef>
#include <cstdlib>
#include <assert.h>
// A memory allocator only for <rent_ptr>
class crisp{
  const static size_t chunk_size = 4096;
  void* field_head_;
public:
  crisp():field_head_(malloc(chunk_size)){}
  void* rental(size_t size)
  {
    assert(size < chunk_size);
  }
};
#endif
