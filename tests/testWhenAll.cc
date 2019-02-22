#define BOOST_TEST_MODULE test_future_queue
#include <boost/test/included/unit_test.hpp>
using namespace boost::unit_test_framework;

#include "future_queue.hpp"

BOOST_AUTO_TEST_SUITE(testWhenAll)

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

BOOST_AUTO_TEST_CASE(singleThreaded) {
  // create queues
  std::vector<cppext::future_queue<MovableDataType>> q;
  for(size_t i = 0; i < 20; ++i) q.emplace_back(10);

  // create notification queue
  auto nq = when_all(q.begin(), q.end());

  // create threads filling the queues
  std::vector<std::thread> threads;
  for(size_t i = 0; i < 20; ++i) {
    threads.emplace_back([q, i]() mutable {
      for(size_t k = 0; k < 10; ++k) {
        usleep(i * 1000);
        MovableDataType x(i * 100 + k);
        q[i].push(std::move(x));
      }
    });
  }

  // receive data after checking the notification queue
  for(size_t k = 0; k < 10; ++k) {
    nq.pop_wait();
    for(size_t i = 0; i < 20; ++i) {
      BOOST_CHECK(q[i].empty() == false);
      MovableDataType x;
      q[i].pop(x);
      BOOST_CHECK_EQUAL(x.value(), i * 100 + k);
    }
  }
  usleep(100000);
  BOOST_CHECK(nq.empty() == true);

  for(auto& t : threads) t.join();
}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_SUITE_END()
