#define BOOST_TEST_MODULE test_future_queue
#include <boost/test/included/unit_test.hpp>
using namespace boost::unit_test_framework;

#include "future_queue.hpp"

BOOST_AUTO_TEST_SUITE(testFront)

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

    // test up to a queue length of 100, start with 1
    for(size_t length=1; length<=100; ++length) {

      future_queue<MovableDataType> q1(length);

      { // in combination with discarding pop()
        for(size_t n=0; n<length; ++n) {
          MovableDataType value( length + n + 120 );
          BOOST_CHECK( q1.push(std::move(value)) == true );
          BOOST_CHECK_EQUAL( value.value(), MovableDataType::undef );
        }
        for(size_t n=0; n<length; ++n) {
          BOOST_CHECK( q1.has_data() == true );
          BOOST_CHECK_EQUAL( q1.front().value(), length + n + 120 );
          q1.pop();
        }
      }

      { // in combination with discarding pop_wait()
        for(size_t n=0; n<length; ++n) {
          MovableDataType value( length + n - 120 );
          BOOST_CHECK( q1.push(std::move(value)) == true );
          BOOST_CHECK_EQUAL( value.value(), MovableDataType::undef );
        }
        for(size_t n=0; n<length; ++n) {
          BOOST_CHECK( q1.has_data() == true );
          BOOST_CHECK_EQUAL( q1.front().value(), length + n - 120 );
          q1.pop_wait();
        }
      }

    }

}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_SUITE_END()
