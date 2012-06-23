#include <boost/chrono.hpp>
#include <boost/unordered_map.hpp>
#include <boost/random.hpp>
#include "map.hpp"
using namespace boost;
using namespace chrono;
using namespace nanahan;

int main(void)
{
  unordered_map<int, int> bmap;
  Map<int, int> nmap;
  static const size_t num = 10000000;
  //*
  std::cout << "insert:" << std::endl;
  {
    boost::mt19937 gen( static_cast<unsigned long>(0) );
    boost::uniform_smallint<> dst( 0, 1 << 30 );
    boost::variate_generator<boost::mt19937&, boost::uniform_smallint<>
                             > rand( gen, dst );
    system_clock::time_point start = system_clock::now();

    for (size_t i =0; i < num; ++i){
      nmap.insert(std::make_pair(rand(),i));
    }
    duration<double> sec = system_clock::now() - start;
    std::cout << "  nanahan\t" << sec.count() << " seconds\n";
  }
  //*/
  //*
  {
    boost::mt19937 gen( static_cast<unsigned long>(0) );
    boost::uniform_smallint<> dst( 0, 1 << 30 );
    boost::variate_generator<boost::mt19937&, boost::uniform_smallint<>
                             > rand( gen, dst );
    system_clock::time_point start = system_clock::now();
    for (size_t i = 0; i < num; ++i){
      bmap.insert(std::make_pair(rand(),i));
    }
    duration<double> sec = system_clock::now() - start;
    std::cout << "  boost  \t" << sec.count() << " seconds\n";
  }
  //*
  //*
  std::cout << "find:" << std::endl;
  {
    boost::mt19937 gen( static_cast<unsigned long>(0) );
    boost::uniform_smallint<> dst( 0, 1 << 30 );
    boost::variate_generator<boost::mt19937&, boost::uniform_smallint<>
                             > rand( gen, dst );
    system_clock::time_point start = system_clock::now();
    for (size_t i =0; i < num; ++i){
      nmap.find(rand());
    }
    duration<double> sec = system_clock::now() - start;
    std::cout << "  nanahan\t" << sec.count() << " seconds\n";
  }
  {
    boost::mt19937 gen( static_cast<unsigned long>(0) );
    boost::uniform_smallint<> dst( 0, 1 << 30 );
    boost::variate_generator<boost::mt19937&, boost::uniform_smallint<>
                             > rand( gen, dst );
    system_clock::time_point start = system_clock::now();
    for (size_t i =0; i < num; ++i){
      bmap.find(rand());
    }
    duration<double> sec = system_clock::now() - start;
    std::cout << "  boost  \t" << sec.count() << " seconds\n";
  }
  //*/
  //*
  std::cout << "erase:" << std::endl;
  {
    boost::mt19937 gen( static_cast<unsigned long>(0) );
    boost::uniform_smallint<> dst( 0, 1 << 30 );
    boost::variate_generator<boost::mt19937&, boost::uniform_smallint<>
                             > rand( gen, dst );
    system_clock::time_point start = system_clock::now();
    for (size_t i = 0; i < num; ++i){
      nmap.erase(nmap.find(rand()));
    }
    duration<double> sec = system_clock::now() - start;
    std::cout << "  nanahan\t" << sec.count() << " seconds\n";
  }
  //*/
  //*
  {
    boost::mt19937 gen( static_cast<unsigned long>(0) );
    boost::uniform_smallint<> dst( 0, 1 << 30 );
    boost::variate_generator<boost::mt19937&, boost::uniform_smallint<>
                             > rand( gen, dst );
    system_clock::time_point start = system_clock::now();
    for (size_t i =0; i < num; ++i){
      size_t r = rand();
      if(bmap.find(r) == bmap.end()){ continue; }
      bmap.erase(bmap.find(r));
    }
    duration<double> sec = system_clock::now() - start;
    std::cout << "  boost  \t" << sec.count() << " seconds\n";
  }
  //*/
}
