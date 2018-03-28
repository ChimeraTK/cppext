#include <features.h>

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

    semaphore() {
      // create new semaphore which is not shared it between processes
      int ret = sem_init(&sem, 0, 0);
      if(ret != 0) throw;
    }

    ~semaphore() {
      sem_destroy(&sem);
    }

    void unlock() {
      int ret = sem_post(&sem);
      if(ret != 0) throw;
      // safety check against misuse
      int value;
      ret = sem_getvalue(&sem, &value);
      if(ret != 0) throw;
      assert(value <= 1);
    }

    bool is_ready() {
      int value;
      int ret = sem_getvalue(&sem, &value);
      if(ret != 0) throw;
      return value > 0;
    }

    void wait_and_reset() {
      int ret = sem_wait(&sem);
      if(ret != 0) throw;
    }

    bool is_ready_and_reset() {
      int ret = sem_trywait(&sem);
      if(ret != 0) {
        if(errno == EAGAIN) {
          return false;
        }
        else {
          throw;
        }
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

#endif
