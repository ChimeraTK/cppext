// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <ranges>
#include <vector>

namespace cppext::ranges {

  /********************************************************************************************************************/

  namespace detail {
    template<template<typename...> class Container>
    struct ToImpl : std::ranges::range_adaptor_closure<ToImpl<Container>> {
      template<std::ranges::input_range R>
      constexpr auto operator()(R&& r) const;
    };
  } // namespace detail

  /********************************************************************************************************************/

  /**
   * Convert a C++ range into the given Container type.
   *
   * Replacement for std::ranges::to, for use on compilers which still lack support (e.g. gcc 13).
   *
   * Example:
   *   std::views::iota(0, 10) | cppext::ranges::to<std::vector>()
   * This will result in an std::vector<int> filled with the numbers 0 to 9.
   *
   * Currently, only std::vector is allowed for use as the Container template parameter.
   */
  template<template<typename...> class Container>
  constexpr auto to() {
    return detail::ToImpl<Container>{};
  }

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  template<template<typename...> class Container>
  template<std::ranges::input_range R>
  constexpr auto detail::ToImpl<Container>::operator()(R&& r) const {
    static_assert(std::is_same_v<Container<int>, std::vector<int>>, "my_ranges::to only supports std::vector for now.");

    using T = std::ranges::range_value_t<R>;
    Container<T> result;

    if constexpr(std::ranges::sized_range<R>) {
      result.reserve(std::ranges::size(r));
    }

    for(auto&& e : r) {
      result.emplace_back(std::forward<decltype(e)>(e));
    }

    return result;
  }

  /********************************************************************************************************************/

} // namespace cppext::ranges
