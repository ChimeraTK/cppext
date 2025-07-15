// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#define BOOST_TEST_MODULE test_ranges
#include <boost/test/included/unit_test.hpp>
using namespace boost::unit_test_framework;

#include "ranges.hpp"

BOOST_AUTO_TEST_SUITE(TestRanges)

/**********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(RangesTo) {
  std::cout << "ranges::to" << std::endl;

  auto vec = std::views::iota(0, 10) | cppext::ranges::to<std::vector>();
  static_assert(std::is_same_v<decltype(vec), std::vector<int>>);

  BOOST_TEST(vec.size() == 10);
  for(size_t i = 0; i < vec.size(); ++i) {
    BOOST_TEST(vec[i] == i);
  }
}

/**********************************************************************************************************************/

BOOST_AUTO_TEST_SUITE_END()
