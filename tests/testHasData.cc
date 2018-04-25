#define BOOST_TEST_MODULE test_future_queue
#include <boost/test/included/unit_test.hpp>
using namespace boost::unit_test_framework;

#include <thread>
#include "future_queue.hpp"

#include "threadsafe_unit_test.hpp"

BOOST_AUTO_TEST_SUITE(testHasData)

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

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(multiThreaded) {

    // test up to a queue length of 100, start with 1
    for(size_t length=1; length<=100; ++length) {

      cppext::future_queue<MovableDataType> q1(length);

      // single value transport
      {
        BOOST_CHECK_TIMEOUT( q1.has_data() == false );
        std::thread sender( [&q1, length] {
          MovableDataType value( length + 42 );
          BOOST_CHECK_TS( q1.push(std::move(value)) == true );
          BOOST_CHECK_EQUAL_TS( value.value(), MovableDataType::undef );
        } );  // end sender thread
        std::thread receiver( [&q1] {
          BOOST_CHECK_TIMEOUT( q1.has_data() == true );
        } );  // end receiver thread
        sender.join();
        receiver.join();
        BOOST_CHECK_TIMEOUT( q1.has_data() == true );
        q1.pop_wait();
        BOOST_CHECK_TIMEOUT( q1.has_data() == false );
      }

    }

}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_SUITE_END()
