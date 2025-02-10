// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <functional>
#include <vector>

namespace cppext { namespace experimental {

  /**
   *  A simple "type erasing" container which works without predefining a list of
   * allowed types and calls a templated functor object or C++14 lambda with
   * auto-type parameter. The functor object / lambda gets instantiated when
   *  filling the container (which might limit the use of this container).
   *
   *  This is just a proof-of-concept and not yet ready for use.
   *
   *  Example code:
   *
   *  struct MyCallable {
   *    template<typename T>
   *    void operator()(T &value) {
   *      std::cout << "HIER " << typeid(T).name() << " -> " << value <<
   * std::endl;
   *    }
   *  };
   *
   *  int main() {
   *    MyCallable x;
   *    type_erasing_vector<MyCallable> v(x);
   *
   *    v.push_back("Hallo123");
   *    v.push_back(std::string("Hallo123"));
   *    v.push_back(42);
   *    v.push_back(123.456);
   *
   *    for(int i=0; i<4; ++i) v.at(i)();
   *    return 0;
   *  }
   *
   */
  template<typename CALLABLE>
  class type_erasing_vector {
   public:
    type_erasing_vector(CALLABLE callable_) : callable(callable_) {}

    template<typename T>
    void push_back(const T& value) {
      functions.push_back(std::bind(callable, value));
    }

    std::function<void(void)> at(size_t index) { return functions.at(index); }

   private:
    CALLABLE callable;
    std::vector<std::function<void(void)>> functions;
  };

}} // namespace cppext::experimental
