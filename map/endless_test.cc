#include "map.hpp"
#include <iostream>
#include <boost/random.hpp>

int main(void) {
  const int size = 10000;
  uint64_t seed = 0;
  while(true) {
    nanahan::Map<int, int> m;
    std::cout << "seed:" << seed << std::endl;
    {
      boost::mt19937 gen( static_cast<unsigned long>(seed) );
      boost::uniform_smallint<> dst( 0, 1 << 30 );
      boost::variate_generator<boost::mt19937&, boost::uniform_smallint<>
                               > rand( gen, dst );
      bool ok = true;
      for(int i = 0; i < size; ++i){
        m.insert(std::make_pair(rand(), i));
        if (!m.sanity_check()) {
          std::cout << "insert: seed :" << seed << " count :" << i << std::endl;
          ok = false;
          break;
        }
      }
      if (!ok) {
        continue;
      }
    }

    {
      boost::mt19937 gen( static_cast<unsigned long>(seed) );
      boost::uniform_smallint<> dst( 0, 1 << 30 );
      boost::variate_generator<boost::mt19937&, boost::uniform_smallint<>
                               > rand( gen, dst );
      bool ok = true;
      for(int i = 0; i < size; ++i){
        //std::cout << i;
        const int target = rand();
        if (m.find(target) == m.end()) {
          std::cout << "find: seed :" << seed << " count :" << i << std::endl;
          ok = false;
          break;
        }
        if (i & 1) {
          m.erase(m.find(target));
          if (!m.sanity_check()) {
            ok = false;
            break;
          }
        }
      }
      if (!ok) {
        continue;
      }
    }

    {
      boost::mt19937 gen( static_cast<unsigned long>(seed) );
      boost::uniform_smallint<> dst( 0, 1 << 30 );
      boost::variate_generator<boost::mt19937&, boost::uniform_smallint<>
                               > rand( gen, dst );
      bool ok = true;
      for(int i = 0; i < size; ++i){
        int target = rand();
        if (i & 1) {
          if (m.find(target) != m.end()) {
            std::cout << "erase: seed :" << seed << " count :" << i << std::endl;
            ok = false;
            break;
          }
          m.insert(std::make_pair(target, i));
          if (m.find(target) == m.end()) {
            std::cout << "reinsert: seed :" << seed << " count :" << i << std::endl;
            ok = false;
            break;
          }
        } else {
          if (m.find(target) == m.end()) {
            std::cout << "not erased: seed :" << seed << " count :" << i << std::endl;
            ok = false;
            break;
          }
        }
        //std::cout << ".";
      }
      if (!ok) {
        continue;
      }

    }
    ++seed;
  }
}
