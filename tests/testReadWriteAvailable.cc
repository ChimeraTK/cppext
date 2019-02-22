#define BOOST_TEST_MODULE test_future_queue
#include <boost/test/included/unit_test.hpp>
using namespace boost::unit_test_framework;

#include "future_queue.hpp"

BOOST_AUTO_TEST_SUITE(testReadWriteAvailable)

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(singleThreaded) {
  for(size_t nbuffers = 1; nbuffers < 100; ++nbuffers) {
    cppext::future_queue<int> q(nbuffers);

    // just single push and pop many times
    BOOST_CHECK_EQUAL(q.read_available(), 0);
    BOOST_CHECK_EQUAL(q.write_available(), nbuffers);
    for(size_t i = 0; i < 100; ++i) {
      q.push(42);
      BOOST_CHECK_EQUAL(q.read_available(), 1);
      BOOST_CHECK_EQUAL(q.write_available(), nbuffers - 1);
      int x;
      BOOST_CHECK(q.pop(x) == true);
      BOOST_CHECK_EQUAL(q.read_available(), 0);
      BOOST_CHECK_EQUAL(q.write_available(), nbuffers);
    }

    // push until queue is full, pop until queue is empty
    for(size_t i = 0; i < nbuffers; ++i) {
      BOOST_CHECK_EQUAL(q.read_available(), i);
      BOOST_CHECK_EQUAL(q.write_available(), nbuffers - i);
      q.push(42);
    }
    BOOST_CHECK_EQUAL(q.read_available(), nbuffers);
    BOOST_CHECK_EQUAL(q.write_available(), 0);
    for(size_t i = 0; i < nbuffers; ++i) {
      BOOST_CHECK_EQUAL(q.read_available(), nbuffers - i);
      BOOST_CHECK_EQUAL(q.write_available(), i);
      int x;
      BOOST_CHECK(q.pop(x) == true);
    }
    BOOST_CHECK_EQUAL(q.read_available(), 0);
    BOOST_CHECK_EQUAL(q.write_available(), nbuffers);
  }
}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_SUITE_END()
