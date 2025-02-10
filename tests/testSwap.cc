// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#define BOOST_TEST_MODULE test_future_queue
#include <boost/test/included/unit_test.hpp>
using namespace boost::unit_test_framework;

#include "future_queue.hpp"

BOOST_AUTO_TEST_SUITE(testSwap)

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testMoving) {
  std::vector<int> a{4, 5, 6};
  std::vector<int> b{9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
  std::vector<int> c{1, 2, 3, 4, 5, 6};

  cppext::future_queue<std::vector<int>> movingQueue(2);

  movingQueue.push(std::move(a));
  BOOST_CHECK_EQUAL(a.size(), 0);

  movingQueue.push(std::move(b));
  BOOST_CHECK_EQUAL(b.size(), 0);

  movingQueue.pop(c);
  BOOST_CHECK_EQUAL(c.size(), 3);
  BOOST_CHECK(c == std::vector<int>({4, 5, 6}));

  movingQueue.pop(a);
  BOOST_CHECK_EQUAL(a.size(), 10);
  BOOST_CHECK(a == std::vector<int>({9, 8, 7, 6, 5, 4, 3, 2, 1, 0}));

  movingQueue.push(std::move(a));
  BOOST_CHECK_EQUAL(a.size(), 0);

  movingQueue.push(std::move(c));
  BOOST_CHECK_EQUAL(c.size(), 0);
}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testSwapping) {
  std::vector<int> a_orig{4, 5, 6};
  std::vector<int> b_orig{9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
  std::vector<int> c_orig{1, 2, 3, 4, 5, 6};
  std::vector<int> d_orig{42, 120};

  std::vector<int> a = a_orig;
  std::vector<int> b = b_orig;
  std::vector<int> c = c_orig;

  cppext::future_queue<std::vector<int>, cppext::SWAP_DATA> swappingQueue(2); // size of 2 means 3 internal buffers

  swappingQueue.push(std::move(a));
  BOOST_CHECK_EQUAL(a.size(), 0);

  swappingQueue.push(std::move(b));
  BOOST_CHECK_EQUAL(b.size(), 0);

  swappingQueue.pop(c); // pop first internal buffer, which is then holding content of c_orig
  BOOST_CHECK(c == a_orig);

  a = d_orig;
  swappingQueue.pop(a); // pop second internal buffer, which is then holding the
                        // content of d_orig
  BOOST_CHECK(a == b_orig);

  swappingQueue.push(std::move(a));
  BOOST_CHECK_EQUAL(a.size(),
      0); // this is coming from the 3rd internal buffer which was not yet used

  swappingQueue.push(std::move(b));
  BOOST_CHECK(b == c_orig);

  swappingQueue.pop();

  swappingQueue.push(std::move(c));
  BOOST_CHECK(c == d_orig);
}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_SUITE_END()
