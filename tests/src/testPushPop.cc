#define BOOST_TEST_MODULE testPushPop
#include <boost/test/included/unit_test.hpp>
using namespace boost::unit_test_framework;

#include "future_queue.h"

BOOST_AUTO_TEST_SUITE(PushPopTestSuite)

// test with a custom data type which is not known to the queue
// we intentionally do not use a convenient standard-like interface to avoid accidental usage of common operators etc.
struct MovableDataType {
    constexpr static int undef = -987654321;
    MovableDataType() {}
    explicit MovableDataType(int value) : _value(value) {}
    MovableDataType(MovableDataType &&other) : _value(other._value) { other._value = undef; }
    MovableDataType& operator=(MovableDataType &&other) {  _value = other._value; other._value = undef; return *this; }
    int value() { return _value; }
  private:
    int _value{undef};
};
constexpr int MovableDataType::undef;

BOOST_AUTO_TEST_CASE(singleThreaded) {

    // test up to a queue length of 100, start with 1
    for(size_t length=1; length<=100; ++length) {

      future_queue<MovableDataType> q1(length);

      // single value transport
      {
        MovableDataType value( length + 42 );
        BOOST_CHECK( q1.push(std::move(value)) == true );
        BOOST_CHECK_EQUAL( value.value(), MovableDataType::undef );
      }
      {
        MovableDataType value;
        BOOST_CHECK( q1.pop(value) == true );
        BOOST_CHECK_EQUAL( value.value(), length + 42 );
      }

      // transport maximum number of values at a time
      for(size_t n=0; n<length; ++n) {
        MovableDataType value( length + n + 120 );
        BOOST_CHECK( q1.push(std::move(value)) == true );
        BOOST_CHECK_EQUAL( value.value(), MovableDataType::undef );
      }
      for(size_t n=0; n<length; ++n) {
        MovableDataType value;
        BOOST_CHECK( q1.pop(value) == true );
        BOOST_CHECK_EQUAL( value.value(), length + n + 120 );
      }

      // test correct behaviour if queue is full resp. empty
      for(size_t n=0; n<length; ++n) {
        MovableDataType value( length + n + 120 );
        BOOST_CHECK( q1.push(std::move(value)) == true );
        BOOST_CHECK_EQUAL( value.value(), MovableDataType::undef );
      }
      { // queue is already full
        MovableDataType value( -666 );
        BOOST_CHECK( q1.push(std::move(value)) == false );
        BOOST_CHECK_EQUAL( value.value(), -666 );
      }
      for(size_t n=0; n<length; ++n) {
        MovableDataType value;
        BOOST_CHECK( q1.pop(value) == true );
        BOOST_CHECK_EQUAL( value.value(), length + n + 120 );
      }
      { // queue is already empty
        MovableDataType value( -777 );
        BOOST_CHECK( q1.pop(value) == false );
        BOOST_CHECK_EQUAL( value.value(), -777 );
      }

    }

}

BOOST_AUTO_TEST_SUITE_END()
