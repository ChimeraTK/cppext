#include <mutex>
#include <condition_variable>

class semaphore {

  public:

    semaphore() : _count(1) {}

    void count_down() {
      std::unique_lock<decltype(_mutex)> lock(_mutex);
      if(_count > 0) --_count;
      if(_count == 0) _condition.notify_one();
    }

    void wait() {
      std::unique_lock<decltype(_mutex)> lock(_mutex);
      while(_count > 0) _condition.wait(lock);
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
