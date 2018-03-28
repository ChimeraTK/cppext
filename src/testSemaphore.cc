#define BOOST_TEST_MODULE test_semaphore
#include <boost/test/included/unit_test.hpp>
using namespace boost::unit_test_framework;

#include "semaphore.hpp"

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

    semaphore sem;

    // unlock the semaphore
    BOOST_CHECK(sem.is_ready() == false);
    sem.count_down();
    // now semaphore should be ready and wait() should not block
    BOOST_CHECK(sem.is_ready() == true);
    sem.wait();
    BOOST_CHECK(sem.is_ready() == true);
    sem.wait_and_reset();
    BOOST_CHECK(sem.is_ready() == false);
    // test reusing the semaphore
    sem.count_down();
    BOOST_CHECK(sem.is_ready() == true);
    sem.wait();
    BOOST_CHECK(sem.is_ready() == true);
    sem.wait_and_reset();
    BOOST_CHECK(sem.is_ready() == false);
    sem.count_down();
    BOOST_CHECK(sem.is_ready() == true);

}

/*********************************************************************************************************************/

// one thread waiting and another counting
BOOST_AUTO_TEST_CASE(multiThreaded) {
    std::cout << "multiThreaded" << std::endl;

    semaphore sem;

    std::thread waiting( [&sem] {
      BOOST_CHECK(sem.is_ready() == false);
      sem.wait();
    } );  // end waiting thread

    std::thread down_counting( [&sem] {
      usleep(10000);
      sem.count_down();
    } );  // end down_counting thread

    down_counting.join();
    waiting.join();
    BOOST_CHECK(sem.is_ready() == true);

}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_SUITE_END()
