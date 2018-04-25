#define BOOST_TEST_MODULE test_future_queue
#include <boost/test/included/unit_test.hpp>
using namespace boost::unit_test_framework;

#include <thread>
#include "future_queue.hpp"

#include "threadsafe_unit_test.hpp"

BOOST_AUTO_TEST_SUITE(testSharing)

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

BOOST_AUTO_TEST_CASE(testCopyConstruct) {

    cppext::future_queue<MovableDataType> queue1(10);
    BOOST_CHECK_EQUAL(queue1.size(), 10);
    queue1.push(MovableDataType(42));

    cppext::future_queue<MovableDataType> queue2(queue1);
    BOOST_CHECK_EQUAL(queue2.size(), 10);
    queue2.push(MovableDataType(43));
    queue1.push(MovableDataType(44));
    queue2.push(MovableDataType(45));
    queue2.push(MovableDataType(46));

    MovableDataType x;
    queue2.pop(x);
    BOOST_CHECK_EQUAL(x.value(), 42);
    queue2.pop(x);
    BOOST_CHECK_EQUAL(x.value(), 43);
    queue2.pop(x);
    BOOST_CHECK_EQUAL(x.value(), 44);
    queue1.pop(x);
    BOOST_CHECK_EQUAL(x.value(), 45);
    queue1.pop(x);
    BOOST_CHECK_EQUAL(x.value(), 46);

    {
      cppext::future_queue<MovableDataType> queue3(12);
      queue3.push(MovableDataType(12345));
      queue1 = queue3;
    }


}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testVector) {

    std::vector<cppext::future_queue<MovableDataType>> vectorOfQueues;

    cppext::future_queue<MovableDataType> queue1(10);
    vectorOfQueues.push_back(queue1);
    cppext::future_queue<MovableDataType> queue2(8);
    vectorOfQueues.emplace_back(queue2);

    BOOST_CHECK_EQUAL(queue1.size(), 10);
    BOOST_CHECK_EQUAL(vectorOfQueues[0].size(), 10);
    BOOST_CHECK_EQUAL(queue2.size(), 8);
    BOOST_CHECK_EQUAL(vectorOfQueues[1].size(), 8);

    queue1.push(MovableDataType(33));
    MovableDataType x;
    vectorOfQueues[0].pop(x);
    BOOST_CHECK_EQUAL(x.value(), 33);

    vectorOfQueues[1].push(MovableDataType(44));
    queue2.pop(x);
    BOOST_CHECK_EQUAL(x.value(), 44);

}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_SUITE_END()
