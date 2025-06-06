// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#define BOOST_TEST_MODULE test_future_queue
#include <boost/test/included/unit_test.hpp>
using namespace boost::unit_test_framework;

#include "future_queue.hpp"
#include "threadsafe_unit_test.hpp"

#include <thread>

BOOST_AUTO_TEST_SUITE(testPushPop)

// test with a custom data type which is not known to the queue
// we intentionally do not use a convenient standard-like interface to avoid
// accidental usage of common operators etc.
struct MovableDataType {
  constexpr static int undef = -987654321;
  MovableDataType() {}
  explicit MovableDataType(int value) : _value(value) {}
  MovableDataType(MovableDataType&& other) : _value(other._value) { other._value = undef; }
  MovableDataType& operator=(MovableDataType&& other) {
    _value = other._value;
    other._value = undef;
    return *this;
  }
  int value() { return _value; }

 private:
  int _value{undef};
};
constexpr int MovableDataType::undef;

/**********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(singleThreaded) {
  std::cout << "singleThreaded" << std::endl;

  // test up to a queue length of 100, start with 1
  for(size_t length = 1; length <= 100; ++length) {
    cppext::future_queue<MovableDataType> q1(length);

    // single value transport
    {
      MovableDataType value(length + 42);
      BOOST_CHECK(q1.push(std::move(value)) == true);
      BOOST_CHECK_EQUAL(value.value(), MovableDataType::undef);
    }
    {
      MovableDataType value;
      BOOST_CHECK(q1.pop(value) == true);
      BOOST_CHECK_EQUAL(value.value(), length + 42);
    }

    // transport maximum number of values at a time
    for(size_t n = 0; n < length; ++n) {
      MovableDataType value(length + n + 120);
      BOOST_CHECK(q1.push(std::move(value)) == true);
      BOOST_CHECK_EQUAL(value.value(), MovableDataType::undef);
    }
    for(size_t n = 0; n < length; ++n) {
      MovableDataType value;
      BOOST_CHECK(q1.pop(value) == true);
      BOOST_CHECK_EQUAL(value.value(), length + n + 120);
    }

    // test correct behaviour if queue is full resp. empty
    for(size_t n = 0; n < length; ++n) {
      MovableDataType value(length + n + 120);
      BOOST_CHECK(q1.push(std::move(value)) == true);
      BOOST_CHECK_EQUAL(value.value(), MovableDataType::undef);
    }
    { // queue is already full
      MovableDataType value(-666);
      BOOST_CHECK(q1.push(std::move(value)) == false);
      BOOST_CHECK_EQUAL(value.value(), -666);
    }
    for(size_t n = 0; n < length; ++n) {
      MovableDataType value;
      BOOST_CHECK(q1.pop(value) == true);
      BOOST_CHECK_EQUAL(value.value(), length + n + 120);
    }
    { // queue is already empty
      MovableDataType value(-777);
      BOOST_CHECK(q1.pop(value) == false);
      BOOST_CHECK_EQUAL(value.value(), -777);
    }
  }
}

/**********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(multiThreaded) {
  std::cout << "multiThreaded" << std::endl;

  // test up to a queue length of 100, start with 1
  for(size_t length = 1; length <= 100; ++length) {
    cppext::future_queue<MovableDataType> q1(length);

    // single value transport
    {
      std::thread sender([&q1, length] {
        MovableDataType value(length + 42);
        BOOST_CHECK_TS(q1.push(std::move(value)) == true);
        BOOST_CHECK_EQUAL_TS(value.value(), MovableDataType::undef);
      }); // end sender thread
      std::thread receiver([&q1, length] {
        MovableDataType value;
        BOOST_CHECK_TIMEOUT(q1.pop(value) == true);
        BOOST_CHECK_EQUAL_TS(value.value(), (signed)length + 42);
      }); // end receiver thread
      sender.join();
      receiver.join();
    }

    // single value transport with pop_wait
    {
      std::thread receiver([&q1, length] {
        MovableDataType value;
        q1.pop_wait(value);
        BOOST_CHECK_EQUAL_TS(value.value(), (signed)length + 42);
      }); // end receiver thread
      std::thread sender([&q1, length] {
        MovableDataType value(length + 42);
        usleep(10000); // intentionally slow down sender
        BOOST_CHECK_TS(q1.push(std::move(value)) == true);
        BOOST_CHECK_EQUAL_TS(value.value(), MovableDataType::undef);
      }); // end sender thread
      sender.join();
      receiver.join();
    }

    // transport maximum number of values at a time
    {
      std::thread sender([&q1, length] {
        for(size_t n = 0; n < length; ++n) {
          MovableDataType value(length + n + 120);
          BOOST_CHECK_TS(q1.push(std::move(value)) == true);
          BOOST_CHECK_EQUAL_TS(value.value(), MovableDataType::undef);
        }
      }); // end sender thread
      std::thread receiver([&q1, length] {
        for(size_t n = 0; n < length; ++n) {
          usleep(100); // intentionally slow down receiver
          MovableDataType value;
          BOOST_CHECK_TIMEOUT(q1.pop(value) == true);
          BOOST_CHECK_EQUAL_TS(value.value(), (signed)length + (signed)n + 120);
        }
      }); // end receiver thread
      sender.join();
      receiver.join();
    }

    // transport values with pop_wait
    {
      std::thread receiver([&q1, length] {
        for(size_t n = 0; n < length; ++n) {
          MovableDataType value;
          q1.pop_wait(value);
          BOOST_CHECK_EQUAL_TS(value.value(), (signed)length + (signed)n + 120);
        }
      }); // end receiver thread
      std::thread sender([&q1, length] {
        for(size_t n = 0; n < length; ++n) {
          usleep(100); // intentionally slow down sender
          MovableDataType value(length + n + 120);
          BOOST_CHECK_TS(q1.push(std::move(value)) == true);
          BOOST_CHECK_EQUAL_TS(value.value(), MovableDataType::undef);
        }
      }); // end sender thread
      sender.join();
      receiver.join();
    }

    // test correct behaviour if queue is full resp. empty
    {
      std::thread sender([&q1, length] {
        for(size_t n = 0; n < length; ++n) {
          MovableDataType value(length + n + 120);
          BOOST_CHECK_TS(q1.push(std::move(value)) == true);
          BOOST_CHECK_EQUAL_TS(value.value(), MovableDataType::undef);
        }
        { // queue is already full
          MovableDataType value(-666);
          BOOST_CHECK_TS(q1.push(std::move(value)) == false);
          BOOST_CHECK_EQUAL_TS(value.value(), -666);
        }
      });            // end sender thread
      sender.join(); // otherwise the queue will never be full
      std::thread receiver([&q1, length] {
        for(size_t n = 0; n < length; ++n) {
          MovableDataType value;
          BOOST_CHECK_TS(q1.pop(value) == true);
          BOOST_CHECK_EQUAL_TS(value.value(), (signed)length + (signed)n + 120);
        }
        { // queue is already empty
          MovableDataType value(-777);
          BOOST_CHECK_TS(q1.pop(value) == false);
          BOOST_CHECK_EQUAL_TS(value.value(), -777);
        }
      }); // end receiver thread
      receiver.join();
    }
  }
}

/**********************************************************************************************************************/

BOOST_AUTO_TEST_SUITE_END()
