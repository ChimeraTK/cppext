// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <condition_variable>

#include <mutex>

namespace cppext {

  class barrier {
   public:
    barrier(size_t nThreads) : _count(nThreads), _nThreads(nThreads) {}

    void wait() {
      std::unique_lock<decltype(_mutex)> lock(_mutex);
      assert(_count > 0);
      --_count;
      if(_count == 0) {
        _barrierReached = true;
        _condition.notify_all();
        _count = _nThreads;
      }
      else {
        _barrierReached = false;
        while(!_barrierReached) _condition.wait(lock);
      }
    }

   private:
    // mutex required for the condition variable to work
    std::mutex _mutex;

    // condition variable used for notification
    std::condition_variable _condition;

    // barrier counter
    size_t _count;

    // flag if barrier is reached by all threads
    bool _barrierReached{false};

    // number of threads
    size_t _nThreads;
  };

} // namespace cppext
