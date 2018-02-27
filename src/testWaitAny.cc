#define BOOST_TEST_MODULE test_future_queue
#include <boost/test/included/unit_test.hpp>
using namespace boost::unit_test_framework;

#include "future_queue.h"

BOOST_AUTO_TEST_SUITE(testWaitAny)

// test with a custom data type which is not known to the queue
// we intentionally do not use a convenient standard-like interface to avoid accidental usage of common operators etc.
struct MovableDataType {
    constexpr static int undef = -987654321;
    MovableDataType() {}
    explicit MovableDataType(int value) : _value(value) {}
    MovableDataType(MovableDataType &&other) : _value(other._value) { other._value = undef; }
    MovableDataType& operator=(MovableDataType &&other) {  _value = other._value; other._value = undef; return *this; }
    int value() const { return _value; }
  private:
    int _value{undef};
};
constexpr int MovableDataType::undef;

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(singleThreaded) {

    // test up to 100 queues up to a queue length of 100
    for(size_t length=1; length<=100; ++length) {
      for(size_t nqueues=1; nqueues<=100; ++nqueues) {

        std::list<future_queue<MovableDataType>> q;
        std::list<std::reference_wrapper<future_queue_base>> qref;
        for(size_t iq=0; iq<nqueues; ++iq) {
          q.emplace_back(length);
          qref.emplace_back(q.back());
        }

        // write once to a single queue and find the change with wait_any
        size_t iq=0;
        for(auto &theQ : q) {
          ++iq;
          MovableDataType value( length*nqueues + iq );
          theQ.push(std::move(value));
          auto id = wait_any(qref);
          BOOST_CHECK( id == theQ.get_id() );
          BOOST_CHECK_EQUAL( theQ.front().value(), length*nqueues + iq );
          MovableDataType readValue;
          theQ.pop(readValue);
          BOOST_CHECK_EQUAL( readValue.value(), length*nqueues + iq );
        }

      }
    }

}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_SUITE_END()
