#define BOOST_TEST_MODULE test_latch
#include <boost/test/included/unit_test.hpp>
using namespace boost::unit_test_framework;

#include "latch.h"

#include <thread>

BOOST_AUTO_TEST_SUITE(TestLatch)

#define BOOST_CHECK_TIMEOUT(condition)                                                                                  \
  bool isOk = false;                                                                                                    \
  for(size_t i=0; i<1000; ++i) {                                                                                        \
    if(condition) { isOk = true; break; }                                                                               \
    usleep(10000);                                                                                                      \
  }                                                                                                                     \
  if(!isOk) BOOST_ERROR("Check with timeout on condition failed: " #condition)

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(singleThreaded) {
    std::cout << "singleThreaded" << std::endl;

    for(size_t count=1; count<=100; ++count) {

      latch l(count);

      // count down 'count' times
      for(size_t i=0; i<count; ++i) {
        BOOST_CHECK(l.is_ready() == false);
        l.count_down();
      }
      // now latch should be ready and wait() should not block
      BOOST_CHECK(l.is_ready() == true);
      l.count_down();
      BOOST_CHECK(l.is_ready() == true);
      l.wait();
      BOOST_CHECK(l.is_ready() == true);
      l.wait_and_reset(count);
      BOOST_CHECK(l.is_ready() == false);
      // test reusing the latch
      for(size_t i=0; i<count; ++i) {
        BOOST_CHECK(l.is_ready() == false);
        l.count_down();
      }
      BOOST_CHECK(l.is_ready() == true);
      l.count_down();
      BOOST_CHECK(l.is_ready() == true);
      l.wait();
      BOOST_CHECK(l.is_ready() == true);
      l.wait_and_reset();
      BOOST_CHECK(l.is_ready() == false);
      l.count_down();
      BOOST_CHECK(l.is_ready() == true);

    }

    // test default constructor
    {
      latch l;
      BOOST_CHECK(l.is_ready() == false);
      l.count_down();
      BOOST_CHECK(l.is_ready() == true);
      l.wait_and_reset(2);
      BOOST_CHECK(l.is_ready() == false);
      l.count_down();
      BOOST_CHECK(l.is_ready() == false);
      for(size_t i=0; i<100; ++i) {
        l.count_down();
        BOOST_CHECK(l.is_ready() == true);
      }
      l.wait_and_reset();
      BOOST_CHECK(l.is_ready() == false);
      l.count_down();
      BOOST_CHECK(l.is_ready() == true);
    }

}

/*********************************************************************************************************************/

// one thread waiting and another counting
BOOST_AUTO_TEST_CASE(multiThreaded) {
    std::cout << "multiThreaded" << std::endl;

    for(size_t count=1; count<=100; ++count) {

      latch l(count);

      std::thread waiting( [&l] {
        BOOST_CHECK(l.is_ready() == false);
        l.wait();
      } );  // end waiting thread

      std::thread down_counting( [&l, count] {
        for(size_t n=0; n<count; ++n) {
          usleep(100);
          l.count_down();
        }
      } );  // end down_counting thread

      down_counting.join();
      waiting.join();
      BOOST_CHECK(l.is_ready() == true);

    }

}

/*********************************************************************************************************************/

// many threads, each decreasing the count by 1 (+ the waiting thread)
BOOST_AUTO_TEST_CASE(manyThreaded) {
    std::cout << "manyThreaded" << std::endl;

    for(size_t count=1; count<=100; ++count) {

      latch l(count);

      std::thread waiting( [&l] {
        l.wait();
        BOOST_CHECK(l.is_ready() == true);
      } );  // end waiting thread

      std::thread down_counting[count];
      for(size_t n=0; n<count; ++n) {
        down_counting[n] = std::thread( [&l] {
          usleep(100);
          l.count_down();
        } );  // end down_counting thread
      }

      for(size_t n=0; n<count; ++n) down_counting[n].join();
      waiting.join();
      BOOST_CHECK(l.is_ready() == true);

    }

}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_SUITE_END()
