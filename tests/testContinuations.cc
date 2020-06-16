#define BOOST_TEST_MODULE test_future_queue
#include <boost/test/included/unit_test.hpp>
using namespace boost::unit_test_framework;

#include "future_queue.hpp"

BOOST_AUTO_TEST_SUITE(testContinuations)

class MyException {};

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testDeferredContinuation) {
  cppext::future_queue<int> q(5);
  std::atomic<size_t> continuationCounter;
  continuationCounter = 0;

  bool throwException = false;

  auto qc = q.then<std::string>(
      [&continuationCounter, &throwException](int x) {
        if(throwException) throw MyException();
        ++continuationCounter;
        return std::to_string(x * 10);
      },
      std::launch::deferred);

  usleep(100000);
  BOOST_CHECK(qc.empty() == true);
  BOOST_CHECK_EQUAL(continuationCounter, 0);
  BOOST_CHECK(qc.pop() == false);
  BOOST_CHECK_EQUAL(continuationCounter, 0);

  q.push(1);
  q.push(2);
  q.push(3);
  q.push(4);
  q.push(5);

  std::string res;

  qc.pop(res);
  BOOST_CHECK_EQUAL(res, "10");

  qc.pop(res);
  BOOST_CHECK_EQUAL(res, "20");

  qc.pop(res);
  BOOST_CHECK_EQUAL(res, "30");

  qc.pop(res);
  BOOST_CHECK_EQUAL(res, "40");

  qc.pop(res);
  BOOST_CHECK_EQUAL(res, "50");

  // test with exception in input queue
  try {
    throw MyException();
  }
  catch(...) {
    q.push_exception(std::current_exception());
  }
  BOOST_CHECK_THROW(qc.pop(res), MyException);
  q.push(6);
  qc.pop(res);
  BOOST_CHECK_EQUAL(res, "60");
  BOOST_CHECK(qc.pop() == false);

  // test with exception thrown in continuation
  throwException = true;
  q.push(7);
  BOOST_CHECK_THROW(qc.pop(res), MyException);
  BOOST_CHECK(qc.pop() == false);

  throwException = false;
  q.push(8);
  qc.pop(res);
  BOOST_CHECK_EQUAL(res, "80");
  BOOST_CHECK(qc.pop() == false);
}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testDeferredContinuation_wait) {
  cppext::future_queue<int> q(5);
  std::atomic<size_t> continuationCounter;
  continuationCounter = 0;

  bool throwException = false;

  auto qc = q.then<std::string>(
      [&continuationCounter, &throwException](int x) {
        if(throwException) throw MyException();
        ++continuationCounter;
        return std::to_string(x * 10);
      },
      std::launch::deferred);

  usleep(100000);
  BOOST_CHECK(qc.empty() == true);
  BOOST_CHECK_EQUAL(continuationCounter, 0);

  std::thread sender([&q] {
    for(int i = 1; i < 6; ++i) {
      usleep(100000);
      q.push(i);
    }
  });

  std::string res;

  BOOST_CHECK_EQUAL(continuationCounter, 0);
  qc.pop_wait(res);
  BOOST_CHECK_EQUAL(continuationCounter, 1);
  BOOST_CHECK_EQUAL(res, "10");

  qc.pop_wait(res);
  BOOST_CHECK_EQUAL(continuationCounter, 2);
  BOOST_CHECK_EQUAL(res, "20");

  qc.pop_wait(res);
  BOOST_CHECK_EQUAL(continuationCounter, 3);
  BOOST_CHECK_EQUAL(res, "30");

  qc.pop_wait(res);
  BOOST_CHECK_EQUAL(continuationCounter, 4);
  BOOST_CHECK_EQUAL(res, "40");

  usleep(200000);
  BOOST_CHECK_EQUAL(continuationCounter, 4);

  qc.pop_wait(res);
  BOOST_CHECK_EQUAL(continuationCounter, 5);
  BOOST_CHECK_EQUAL(res, "50");

  // test with exception in input queue
  try {
    throw MyException();
  }
  catch(...) {
    q.push_exception(std::current_exception());
  }
  BOOST_CHECK_THROW(qc.pop_wait(res), MyException);
  q.push(6);
  qc.pop_wait(res);
  BOOST_CHECK_EQUAL(res, "60");
  BOOST_CHECK(qc.pop() == false);

  // test with exception thrown in continuation
  throwException = true;
  q.push(7);
  BOOST_CHECK_THROW(qc.pop_wait(res), MyException);
  BOOST_CHECK(qc.pop() == false);

  throwException = false;
  q.push(8);
  qc.pop_wait(res);
  BOOST_CHECK_EQUAL(res, "80");
  BOOST_CHECK(qc.pop() == false);

  sender.join();
}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testAsyncContinuation) {
  cppext::future_queue<int> q(5);
  std::atomic<bool> throwException{false};

  auto qc = q.then<std::string>(
      [&throwException](int x) {
        usleep(100000);
        if(throwException) throw MyException();
        return std::to_string(x * 10);
      },
      std::launch::async);

  q.push(1);
  q.push(2);
  q.push(3);
  q.push(4);
  q.push(5);

  std::string res;

  BOOST_CHECK(qc.empty() == true);
  qc.pop_wait(res);
  BOOST_CHECK_EQUAL(res, "10");

  BOOST_CHECK(qc.empty() == true);
  qc.pop_wait(res);
  BOOST_CHECK_EQUAL(res, "20");

  BOOST_CHECK(qc.empty() == true);
  qc.pop_wait(res);
  BOOST_CHECK_EQUAL(res, "30");

  BOOST_CHECK(qc.empty() == true);
  qc.pop_wait(res);
  BOOST_CHECK_EQUAL(res, "40");

  BOOST_CHECK(qc.empty() == true);
  qc.pop_wait(res);
  BOOST_CHECK_EQUAL(res, "50");

  // test with exception in input queue
  try {
    throw MyException();
  }
  catch(...) {
    q.push_exception(std::current_exception());
  }
  BOOST_CHECK_THROW(qc.pop_wait(res), MyException);
  q.push(6);
  qc.pop_wait(res);
  BOOST_CHECK_EQUAL(res, "60");
  BOOST_CHECK(qc.pop() == false);

  // test with exception thrown in continuation
  throwException = true;
  q.push(7);
  BOOST_CHECK_THROW(qc.pop_wait(res), MyException);
  BOOST_CHECK(qc.pop() == false);

  throwException = false;
  q.push(8);
  qc.pop_wait(res);
  BOOST_CHECK_EQUAL(res, "80");
  BOOST_CHECK(qc.pop() == false);
}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testDeferredContinuation_void) {
  cppext::future_queue<void> q(5);
  std::atomic<size_t> continuationCounter;
  continuationCounter = 0;
  bool throwException = false;

  auto qc = q.then<void>(
      [&continuationCounter, &throwException] {
        if(throwException) throw MyException();
        ++continuationCounter;
        return;
      },
      std::launch::deferred);

  usleep(100000);
  BOOST_CHECK(qc.empty() == true);
  BOOST_CHECK_EQUAL(continuationCounter, 0);
  BOOST_CHECK(qc.pop() == false);
  BOOST_CHECK_EQUAL(continuationCounter, 0);

  q.push();
  q.push();
  q.push();
  q.push();
  q.push();

  BOOST_CHECK(qc.pop() == true);
  BOOST_CHECK(qc.pop() == true);
  BOOST_CHECK(qc.pop() == true);
  BOOST_CHECK(qc.pop() == true);
  BOOST_CHECK(qc.pop() == true);

  // test with exception in input queue
  try {
    throw MyException();
  }
  catch(...) {
    q.push_exception(std::current_exception());
  }
  BOOST_CHECK_THROW(qc.pop(), MyException);
  q.push();
  BOOST_CHECK(qc.pop() == true);
  BOOST_CHECK(qc.pop() == false);

  // test with exception thrown in continuation
  throwException = true;
  q.push();
  BOOST_CHECK_THROW(qc.pop(), MyException);
  BOOST_CHECK(qc.pop() == false);

  throwException = false;
  q.push();
  BOOST_CHECK(qc.pop() == true);
  BOOST_CHECK(qc.pop() == false);
}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_SUITE_END()
