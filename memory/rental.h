#ifndef RENTAL_H
#define RENTAL_H
#include <cstdint>
#include <string>
#include "crisp.h"

void* rental(size_t size){
  return global_crisp.rental(size);
}

template<typename T>
class rent_ptr{
  void* ptr_;
public:
  void* get(){return ptr_;}
  void* get() const {return ptr_;}
  rent_ptr(T&& orig):ptr_(rental(sizeof(orig))){ptr_=orig; }
};

#endif
