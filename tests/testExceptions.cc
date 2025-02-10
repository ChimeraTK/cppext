// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#define BOOST_TEST_MODULE test_future_queue
#include <boost/test/included/unit_test.hpp>
using namespace boost::unit_test_framework;

#include "future_queue.hpp"
#include "threadsafe_unit_test.hpp"

#include <thread>

BOOST_AUTO_TEST_SUITE(testExceptions)

struct MyException {
  int value;
};

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testExceptions) {
  // setup a queue
  cppext::future_queue<std::string> q(8);

  // push a value into the queue
  q.push("Hello World");

  // push an exception into the queue
  try {
    MyException e1;
    e1.value = 42;
    throw e1;
  }
  catch(MyException&) {
    q.push_exception(std::current_exception());
  }

  // push another value
  q.push("After exception");

  // push a second exception into the queue
  try {
    MyException e2;
    e2.value = 43;
    throw e2;
  }
  catch(MyException&) {
    q.push_exception(std::current_exception());
  }

  // push a third exception into the queue
  try {
    MyException e3;
    e3.value = 44;
    throw e3;
  }
  catch(MyException&) {
    q.push_exception(std::current_exception());
  }

  // pop the first value from the queue
  std::string v;
  q.pop(v);
  BOOST_CHECK_EQUAL(v, "Hello World");

  // pop first exception from queue
  try {
    q.pop(v);
    BOOST_ERROR("Exception expected.");
  }
  catch(MyException& ep1) {
    BOOST_CHECK_EQUAL(ep1.value, 42);
  }

  // pop the second value from the queue
  q.pop(v);
  BOOST_CHECK_EQUAL(v, "After exception");

  // pop second exception from queue
  try {
    q.pop_wait(v);
    BOOST_ERROR("Exception expected.");
  }
  catch(MyException& ep2) {
    BOOST_CHECK_EQUAL(ep2.value, 43);
  }

  // checkout third exception on queue without popping it
  BOOST_CHECK(q.empty() == false);

  try {
    v = q.front();
    BOOST_ERROR("Exception expected.");
  }
  catch(MyException& ef) {
    BOOST_CHECK_EQUAL(ef.value, 44);
  }

  BOOST_CHECK(q.empty() == false);

  // pop third exception from queue
  try {
    q.pop(v);
    BOOST_ERROR("Exception expected.");
  }
  catch(MyException& ep3) {
    BOOST_CHECK_EQUAL(ep3.value, 44);
  }

  BOOST_CHECK(q.empty() == true);
}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_SUITE_END()
