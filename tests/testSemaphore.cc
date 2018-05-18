#define BOOST_TEST_MODULE test_semaphore
#include <boost/test/included/unit_test.hpp>
using namespace boost::unit_test_framework;

#include "semaphore.hpp"

#include "threadsafe_unit_test.hpp"

#include <thread>

BOOST_AUTO_TEST_SUITE(TestLatch)

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(singleThreaded) {
    std::cout << "singleThreaded" << std::endl;

    cppext::semaphore sem;

    // unlock the semaphore
    BOOST_CHECK(sem.is_ready() == false);
    sem.unlock();
    // now semaphore should be ready and wait() should not block
    BOOST_CHECK(sem.is_ready() == true);
    sem.wait_and_reset();
    BOOST_CHECK(sem.is_ready() == false);
    // test reusing the semaphore
    sem.unlock();
    BOOST_CHECK(sem.is_ready() == true);
    sem.wait_and_reset();
    BOOST_CHECK(sem.is_ready() == false);
    sem.unlock();
    BOOST_CHECK(sem.is_ready() == true);

}

/*********************************************************************************************************************/

// one thread waiting and another counting
BOOST_AUTO_TEST_CASE(multiThreaded) {
    std::cout << "multiThreaded" << std::endl;

    cppext::semaphore sem;

    std::thread waiting( [&sem] {
      BOOST_CHECK_TS(sem.is_ready() == false);
      sem.wait_and_reset();
    } );  // end waiting thread

    std::thread down_counting( [&sem] {
      usleep(100000);
      sem.unlock();
    } );  // end down_counting thread

    down_counting.join();
    waiting.join();
    BOOST_CHECK_TS(sem.is_ready() == false);

}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_SUITE_END()
