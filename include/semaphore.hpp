#ifndef FUTURE_QUEUE_SEMAPHORE_HPP
#define FUTURE_QUEUE_SEMAPHORE_HPP

#ifdef __linux__
#include <features.h>
#endif

#include <cassert>
#include <system_error>

/*
 * Two implementations: one is based on the posix semaphore (sem_wait etc.), the other is pure C++ 11. The posix
 * implementation is considered to be more efficient, so we use the pure C++ 11 implementation only as a fallback.
 */

#if _POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600
/**********************************************************************************************************************
 * POSIX-based implementation follows
 *********************************************************************************************************************/

#include <semaphore.h>

class semaphore {

  public:

    /** Create a new semaphore which is initially not ready */
    semaphore() {
      // create new semaphore which is not shared it between processes
      int ret = sem_init(&sem, 0, 0);
      if(ret != 0) throw std::system_error(std::error_code(errno, std::generic_category()));
    }

    /** Destroy semaphore. No other thread must be waiting on this semaphore when the destructor is called. */
    ~semaphore() {
      sem_destroy(&sem);
    }

    /** Make the semaphore ready. If another thread is currently waiting for the semaphore (see wait_and_reset()), it
     *  will be unblocked. Calling unlock() on a semaphore which is already ready is not allowed. */
    void unlock() {
      int ret = sem_post(&sem);
      if(ret != 0) throw std::system_error(std::error_code(errno, std::generic_category()));
      // safety check against misuse
      int value;
      ret = sem_getvalue(&sem, &value);
      if(ret != 0 || value > 1) throw std::system_error(std::error_code(errno, std::generic_category()));
    }

    /** Check if the semaphore is currently ready. Does not block or alter the state of the semaphore. */
    bool is_ready() {
      int value;
      int ret = sem_getvalue(&sem, &value);
      if(ret != 0) throw std::system_error(std::error_code(errno, std::generic_category()));
      return value > 0;
    }

    /** Block the calling thread until the semaphore gets ready, i.e. unlock() is called by another thread. If the
     *  semaphore is already ready when entering this function, it will not block. The semaphore will be atomically
     *  locked again, so if multiple threads are waiting on the same semaphore at the same time, only one of them
     *  will unblock. */
    void wait_and_reset() {
      int ret = sem_wait(&sem);
      if(ret != 0) throw std::system_error(std::error_code(errno, std::generic_category()));
    }

    /** Check if the semaphore is ready. If not, return false immediately. If yes, atomically lock the semaphore and
     *  return true. */
    bool is_ready_and_reset() {
      int ret = sem_trywait(&sem);
      if(ret != 0) {
        if(errno != EAGAIN) throw std::system_error(std::error_code(errno, std::generic_category()));
        return false;
      }
      return true;
    }

  private:

    // the POSIX semaphore object
    sem_t sem;

};

#else

/**********************************************************************************************************************
 * Pure C++ 11 implementation follows
 *********************************************************************************************************************/

#include <mutex>
#include <condition_variable>

class semaphore {

  public:

    semaphore() : _count(1) {}

    void unlock() {
      std::unique_lock<decltype(_mutex)> lock(_mutex);
      assert(_count > 0);
      --_count;
      if(_count == 0) _condition.notify_one();
    }

    bool is_ready() {
      std::unique_lock<decltype(_mutex)> lock(_mutex);
      return _count == 0;
    }

    void wait_and_reset() {
      std::unique_lock<decltype(_mutex)> lock(_mutex);
      while(_count > 0) _condition.wait(lock);
      _count = 1;
    }

    bool is_ready_and_reset() {
      std::unique_lock<decltype(_mutex)> lock(_mutex);
      if(_count == 0) {
        _count = 1;
        return true;
      }
      return false;
    }

  private:

    // mutex required for the condition variable to work
    std::mutex _mutex;

    // condition variable used for notification
    std::condition_variable _condition;

    // semaphore counter
    size_t _count;

};

#endif // POSIX feature switch

#endif // FUTURE_QUEUE_SEMAPHORE_HPP
