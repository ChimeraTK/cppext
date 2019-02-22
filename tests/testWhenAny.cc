#define BOOST_TEST_MODULE test_future_queue
#include <boost/test/included/unit_test.hpp>
using namespace boost::unit_test_framework;

#include "future_queue.hpp"

BOOST_AUTO_TEST_SUITE(testWaitAny)

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
  int value() const { return _value; }

 private:
  int _value{undef};
};
constexpr int MovableDataType::undef;

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(singleThreaded) { // note: multi-threaded test is part of the stresstest

  // test up to 100 queues up to a queue length of 100
  for(size_t length = 2; length <= 10; ++length) {
    for(size_t nqueues = 1; nqueues <= 10; ++nqueues) {
      std::list<cppext::future_queue<MovableDataType>> q;
      for(size_t iq = 0; iq < nqueues; ++iq) {
        q.emplace_back(length);
      }

      // create notification queue
      auto nq = when_any(q.begin(), q.end());

      // write once to a single queue and find the change with wait_any
      size_t iq = 0;
      for(auto& theQ : q) {
        MovableDataType value(length * nqueues + iq);
        theQ.push(std::move(value));
        size_t id;
        BOOST_CHECK(nq.pop(id));
        BOOST_CHECK(id == iq);
        MovableDataType readValue;
        BOOST_CHECK(theQ.pop(readValue));
        BOOST_CHECK_EQUAL(readValue.value(), length * nqueues + iq);
        ++iq;
      }

      // write a mixed sequece to the queues and check that the order is
      // properly reflected in the notification queue
      for(size_t i = 0; i < length; ++i) {
        iq = 0;
        for(auto& theQ : q) {
          MovableDataType value(length * nqueues + i + iq);
          if(iq % 2 == 0) {
            BOOST_CHECK(theQ.push(std::move(value)));
          }
          else {
            BOOST_CHECK(theQ.push_overwrite(std::move(value)));
          }
          ++iq;
        }
      }
      // all queues are now full, now overwrite the last written value in the
      // first queue
      {
        MovableDataType value(42);
        BOOST_CHECK(q.front().push_overwrite(std::move(value)) == false);
      }
      // check notifications in the notification queue (the overwrite in the
      // first queue is not visible there!)
      for(size_t i = 0; i < length; ++i) {
        iq = 0;
        for(auto& theQ : q) {
          size_t id;
          nq.pop_wait(id);
          BOOST_CHECK(id == iq);
          MovableDataType readValue;
          BOOST_CHECK(theQ.pop(readValue));
          if(i < length - 1 || iq > 0) {
            BOOST_CHECK_EQUAL(readValue.value(), length * nqueues + i + iq);
          }
          else {
            BOOST_CHECK_EQUAL(readValue.value(), 42); // was overwritten!
          }
          ++iq;
        }
      }
    }
  }
}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(calledWithFilledQueues) {
  // test calling when_any with queues that already contain data

  // test up to 100 queues up to a queue length of 100
  for(size_t length = 2; length <= 10; ++length) {
    for(size_t nqueues = 1; nqueues <= 10; ++nqueues) {
      std::vector<cppext::future_queue<MovableDataType>> q;
      for(size_t iq = 0; iq < nqueues; ++iq) q.emplace_back(length);

      // write once to a single queue
      for(size_t iq = 0; iq < nqueues; ++iq) {
        for(size_t i = 0; i < iq; ++i) { // note: writes might get rejected
                                         // since the queue is too short
          MovableDataType value(length * nqueues + iq + 3 * i);
          if(iq % 2 == 0) {
            q[iq].push_overwrite(std::move(value));
          }
          else {
            q[iq].push(std::move(value));
          }
        }
      }

      // create notification queue
      auto nq = when_any(q.begin(), q.end());

      // find the previously written data with wait_any
      for(size_t iq = 0; iq < nqueues; ++iq) {
        for(size_t i = 0; i < std::min(iq, length); ++i) { // only check until length of the queue
          size_t id;
          BOOST_CHECK(nq.pop(id));
          BOOST_CHECK(id == iq);
          MovableDataType readValue;
          BOOST_CHECK(q[iq].pop(readValue));
          // don't compare the value since in case of push_overwrite this gets
          // complicated...
        }
      }

      // write a mixed sequece to the queues and check that the order is
      // properly reflected in the notification queue
      for(size_t i = 0; i < length; ++i) {
        for(size_t iq = 0; iq < nqueues; ++iq) {
          MovableDataType value(length * nqueues + i + iq);
          BOOST_CHECK(q[iq].push(std::move(value)));
        }
      }
      // all queues are now full, now overwrite the last written value in the
      // first queue
      {
        MovableDataType value(42);
        BOOST_CHECK(q.front().push_overwrite(std::move(value)) == false);
      }
      // check notifications in the notification queue (the overwrite in the
      // first queue is not visible there!)
      for(size_t i = 0; i < length; ++i) {
        for(size_t iq = 0; iq < nqueues; ++iq) {
          size_t id;
          nq.pop_wait(id);
          BOOST_CHECK(id == iq);
          MovableDataType readValue;
          BOOST_CHECK(q[iq].pop(readValue));
          if(i < length - 1 || iq > 0) {
            BOOST_CHECK_EQUAL(readValue.value(), length * nqueues + i + iq);
          }
          else {
            BOOST_CHECK_EQUAL(readValue.value(), 42); // was overwritten!
          }
        }
      }
    }
  }
}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_SUITE_END()
