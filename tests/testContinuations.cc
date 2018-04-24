#define BOOST_TEST_MODULE test_future_queue
#include <boost/test/included/unit_test.hpp>
using namespace boost::unit_test_framework;

#include "future_queue.hpp"

BOOST_AUTO_TEST_SUITE(testContinuations)

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testLazyContinuation) {

    future_queue<int> q(5);

    auto qc = q.then<std::string>( [](int x) { return std::to_string(x*10); }, std::launch::deferred );

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

BOOST_AUTO_TEST_CASE(testAsyncContinuation) {

    future_queue<int> q(5);

    auto qc = q.then<std::string>( [](int x) { return std::to_string(x*10); }, std::launch::async );

    q.push(1);
    q.push(2);
    q.push(3);
    q.push(4);
    q.push(5);

    std::string res;

    qc.pop_wait(res);
    BOOST_CHECK_EQUAL( res, "10" );

    qc.pop_wait(res);
    BOOST_CHECK_EQUAL( res, "20" );

    qc.pop_wait(res);
    BOOST_CHECK_EQUAL( res, "30" );

    qc.pop_wait(res);
    BOOST_CHECK_EQUAL( res, "40" );

    qc.pop_wait(res);
    BOOST_CHECK_EQUAL( res, "50" );

}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_SUITE_END()
