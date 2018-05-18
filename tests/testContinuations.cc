#define BOOST_TEST_MODULE test_future_queue
#include <boost/test/included/unit_test.hpp>
using namespace boost::unit_test_framework;

#include "future_queue.hpp"

BOOST_AUTO_TEST_SUITE(testContinuations)

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testDeferredContinuation) {

    cppext::future_queue<int> q(5);
    std::atomic<size_t> continuationCounter;
    continuationCounter = 0;

    auto qc = q.then<std::string>( [&continuationCounter](int x) {
      ++continuationCounter;
      return std::to_string(x*10);
    }, std::launch::deferred );

    usleep(100000);
    BOOST_CHECK(qc.empty() == true);
    BOOST_CHECK_EQUAL( continuationCounter, 0 );
    BOOST_CHECK(qc.pop() == false);
    BOOST_CHECK_EQUAL( continuationCounter, 0 );

    q.push(1);
    q.push(2);
    q.push(3);
    q.push(4);
    q.push(5);

    std::string res;

    qc.pop(res);
    BOOST_CHECK_EQUAL( res, "10" );

    qc.pop(res);
    BOOST_CHECK_EQUAL( res, "20" );

    qc.pop(res);
    BOOST_CHECK_EQUAL( res, "30" );

    qc.pop(res);
    BOOST_CHECK_EQUAL( res, "40" );

    qc.pop(res);
    BOOST_CHECK_EQUAL( res, "50" );

}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testDeferredContinuation_wait) {

    cppext::future_queue<int> q(5);
    std::atomic<size_t> continuationCounter;
    continuationCounter = 0;

    auto qc = q.then<std::string>( [&continuationCounter](int x) {
      ++continuationCounter;
      return std::to_string(x*10);
    }, std::launch::deferred );

    usleep(100000);
    BOOST_CHECK(qc.empty() == true);
    BOOST_CHECK_EQUAL( continuationCounter, 0 );

    std::thread sender( [&q] {
      for(int i=1; i<6; ++i) {
        usleep(100000);
        q.push(i);
      }
    } );

    std::string res;

    BOOST_CHECK_EQUAL( continuationCounter, 0 );
    qc.pop_wait(res);
    BOOST_CHECK_EQUAL( continuationCounter, 1 );
    BOOST_CHECK_EQUAL( res, "10" );

    qc.pop_wait(res);
    BOOST_CHECK_EQUAL( continuationCounter, 2 );
    BOOST_CHECK_EQUAL( res, "20" );

    qc.pop_wait(res);
    BOOST_CHECK_EQUAL( continuationCounter, 3 );
    BOOST_CHECK_EQUAL( res, "30" );

    qc.pop_wait(res);
    BOOST_CHECK_EQUAL( continuationCounter, 4 );
    BOOST_CHECK_EQUAL( res, "40" );

    usleep(200000);
    BOOST_CHECK_EQUAL( continuationCounter, 4 );

    qc.pop_wait(res);
    BOOST_CHECK_EQUAL( continuationCounter, 5 );
    BOOST_CHECK_EQUAL( res, "50" );

    sender.join();

}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testAsyncContinuation) {

    cppext::future_queue<int> q(5);

    auto qc = q.then<std::string>( [](int x) { usleep(100000); return std::to_string(x*10); }, std::launch::async );

    q.push(1);
    q.push(2);
    q.push(3);
    q.push(4);
    q.push(5);

    std::string res;

    BOOST_CHECK( qc.empty() == true);
    qc.pop_wait(res);
    BOOST_CHECK_EQUAL( res, "10" );

    BOOST_CHECK( qc.empty() == true);
    qc.pop_wait(res);
    BOOST_CHECK_EQUAL( res, "20" );

    BOOST_CHECK( qc.empty() == true);
    qc.pop_wait(res);
    BOOST_CHECK_EQUAL( res, "30" );

    BOOST_CHECK( qc.empty() == true);
    qc.pop_wait(res);
    BOOST_CHECK_EQUAL( res, "40" );

    BOOST_CHECK( qc.empty() == true);
    qc.pop_wait(res);
    BOOST_CHECK_EQUAL( res, "50" );

}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testLazyContinuation_void) {

    cppext::future_queue<void> q(5);
    std::atomic<size_t> continuationCounter;
    continuationCounter = 0;

    auto qc = q.then<void>( [&continuationCounter] {
      ++continuationCounter;
      return;
    }, std::launch::deferred );

    usleep(100000);
    BOOST_CHECK(qc.empty() == true);
    BOOST_CHECK_EQUAL( continuationCounter, 0 );
    BOOST_CHECK(qc.pop() == false);
    BOOST_CHECK_EQUAL( continuationCounter, 0 );

    q.push();
    q.push();
    q.push();
    q.push();
    q.push();

    BOOST_CHECK( qc.pop() == true );
    BOOST_CHECK( qc.pop() == true );
    BOOST_CHECK( qc.pop() == true );
    BOOST_CHECK( qc.pop() == true );
    BOOST_CHECK( qc.pop() == true );

}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_SUITE_END()
