#include <boost/chrono.hpp>
#include <string>
#include <boost/unordered_map.hpp>
#include <boost/lexical_cast.hpp>
#include <tr1/unordered_map>
#include <boost/random.hpp>
#include <google/sparse_hash_map>
//#include <google/dense_hash_map>
#include "map.hpp"
using namespace boost::chrono;
using namespace nanahan;
using namespace google;

enum eval_name{
  insert,
  find,
  erase
};

template <typename TargetType>
void measure_speed(const std::string& name,TargetType& target, size_t num, eval_name doing){
  boost::mt19937 gen( static_cast<unsigned long>(0) );
  boost::uniform_smallint<> dst( 0, 1 << 30 );
  boost::variate_generator<boost::mt19937&, boost::uniform_smallint<>
                           > rand( gen, dst );
  system_clock::time_point start = system_clock::now();

  switch(doing){
  case insert:
    for (size_t i =0; i < num; ++i){
      target.insert(std::make_pair(rand(),i));
    }
    break;
  case find:
    for (size_t i =0; i < num; ++i){
      target.find(rand());
    }
    break;
  case erase:
    for (size_t i =0; i < num; ++i){
      size_t r = rand();
      if(target.find(r) == target.end()){ continue; }
      target.erase(target.find(r));
    }
    break;
  default:
    std::cout << "invalid target";
  }

  duration<double> sec = system_clock::now() - start;
  std::cout << " " << name << ":" << num <<  "\t" << int(double(num) / sec.count()) << " qps\n";
}

int main(void)
{
  for(int i =  1000000; i <= 10000000; i += 1000000){
    for(int j =0; j<10; ++j){
      boost::unordered_map<int, int> bmap;
      Map<int, int> nmap;
      std::tr1::unordered_map<int, int> tmap;
      sparse_hash_map<int, int> gsmap;
      //dense_hash_map<int, int> gdmap;

      std::cout << "insert:" << std::endl;
      measure_speed("nanahan",nmap, i, insert);
      measure_speed("boost  ",bmap, i, insert);
      measure_speed("tr1    ",tmap, i, insert);
      measure_speed("g_sparse",gsmap, i, insert);
      //measure_speed("g_dense",gdmap, i, insert);

      std::cout << "find:" << std::endl;
      measure_speed("nanahan",nmap, i, find);
      measure_speed("boost  ",bmap, i, find);
      measure_speed("tr1    ",tmap, i, find);
      measure_speed("g_sparse",gsmap, i, find);
      //measure_speed("g_dense",gdmap, i, find);

      std::cout << "erase:" << std::endl;
      measure_speed("nanahan",nmap, i, erase);
      measure_speed("boost  ",bmap, i, erase);
      measure_speed("tr1    ",tmap, i, erase);
      measure_speed("g_sparse",gsmap, i, erase);
      //measure_speed("g_dense",gdmap, i, erase);
    }
  }
}
