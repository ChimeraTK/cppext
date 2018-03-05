#include <thread>
#include <iostream>
#include <atomic>
#include <vector>
#include <assert.h>
#include <unistd.h>

#include "latch.h"

// Base class for future_queue which does not depend on the template argument. For a description see future_queue.
class future_queue_base {

  public:

    future_queue_base(size_t length)
    : id(get_next_id()),
      nBuffers(length+1),
      latches(length+1),
      writeFlags(length+1),
      updateIds(length+1)
    {
      // buffer 0 will be the first buffer to use
      readIndex = 0;
      writeIndex = 0;

      hasFrontOwnership = false;
    }

    // number of push operations which can be performed before the queue is full
    // Note: in a multi-producer context the result will be inaccurate!
    size_t write_available() const {
      size_t l_writeIndex = writeIndex;     // local copies to ensure consistency
      size_t l_readIndex = readIndex;
      // one buffer is always kept free so the receiver can always wait on a latch
      if(l_writeIndex < l_readIndex) {
        return l_readIndex - l_writeIndex - 1;
      }
      else {
        return nBuffers + l_readIndex - l_writeIndex - 1;
      }
    }

    // Number of pop operations which can be performed before the queue is empty. Note that the result can be inaccurate
    // in case the sender uses push_overwrite(). If a guarantee is required that a readable element is present before
    // accessing it through pop() or front(), use has_data().
    // Note: in a multi-producer context the result will be inaccurate!
    size_t read_available() {
      size_t l_writeIndex = writeIndex;     // local copies to ensure consistency
      size_t l_readIndex = readIndex;
      if(l_readIndex <= l_writeIndex) {
        return l_writeIndex - l_readIndex;
      }
      else {
        return nBuffers + l_writeIndex - l_readIndex;
      }
    }

    // Check for the presence of readable data in the queue.
    bool has_data() {
      if(hasFrontOwnership) return true;
      if(latches[readIndex].is_ready_and_reset()) {
        hasFrontOwnership = true;
        return true;
      }
      return false;
    }

    // Class for the process-unique id_t, to prevent exposing implementation details of the id_t to the public interface
    class id_t {
      public:
        id_t(const id_t &other) : _id(other._id) {}
        bool operator==(const id_t &rhs) const { return rhs._id == _id; }
        bool operator!=(const id_t &rhs) const { return rhs._id != _id; }
        bool operator<(const id_t &rhs) const { return rhs._id < _id; }
        bool operator>(const id_t &rhs) const { return rhs._id > _id; }
        bool operator<=(const id_t &rhs) const { return rhs._id <= _id; }
        bool operator>=(const id_t &rhs) const { return rhs._id >= _id; }
        id_t& operator=(const id_t &rhs) { _id = rhs._id; return *this; }

        static id_t nullid() { return {0}; }

      private:
        id_t(size_t id) : _id(id) {}
        size_t _id;
        friend class future_queue_base;
    };

    // return a process-unique id_t of this
    const id_t& get_id() const {
      return id;
    }

  protected:

    // process-unique id_t of this queue
    id_t id;

    // the number of buffers we have allocated
    size_t nBuffers;

    // vector of latches corresponding to the buffers which allows the receiver to wait for new data
    std::vector<latch> latches;

    // vector of write-reserved flags
    std::vector<std::atomic<size_t>> writeFlags;

    // vector of update ids corresponding to the buffers
    std::vector<size_t> updateIds;

    // index of the element which will be next written
    std::atomic<size_t> writeIndex;

    // index of the element which will be next read
    std::atomic<size_t> readIndex;

    // Flag if the receiver has already ownership over the front element. This flag may only be used by the receiver.
    bool hasFrontOwnership;

    // return the next index
    size_t nextIndex(size_t index) const {
      return (index+1) % nBuffers;
    }

    // return the preious index
    size_t previousIndex(size_t index) const {
      return (index+nBuffers-1) % nBuffers;
    }

    // return the next write index (without changing the member) - no checking if free buffers are available is done!
    size_t nextWriteIndex() {
      return (writeIndex+1) % nBuffers;
    }

    // return the previous write index (without changing the member) - no checking of anything is done!
    size_t previousWriteIndex() {
      return (writeIndex+nBuffers-1) % nBuffers;
    }

    // return the next read index (without changing the member) - no checking if full buffers are available is done!
    size_t nextReadIndex() {
      return (readIndex+1) % nBuffers;
    }

    // return next available update id (used in wait_any to determine the order)
    static size_t get_next_update_id() {
      static std::atomic<size_t> updateId{0};
      ++updateId;
      return updateId;
    }

    static size_t get_max_update_id() {
      return std::numeric_limits<size_t>::max();
    }

    // Only allowed to call after has_data() has returned true!
    size_t get_update_id() const {
      return updateIds[readIndex];
    }

    // reserve next available write slot. Returns false if no free slot is available or true on success.
    bool obtain_write_slot(size_t &index) {
      // last possble index for next write operation is just before the read index - one buffer must be kept free all times
      size_t writeIndexMax = previousIndex(readIndex);
      // try all queue slots until a free one is found
      for(index = writeIndex; index != writeIndexMax; index = nextIndex(index)) {
        auto flag = ++writeFlags[index];
        if(flag == 1) {
          // atomically move write index forward
          while(true) {
            size_t oldWriteIndex = writeIndex;
            size_t newWriteIndex = nextIndex(oldWriteIndex);
            bool done = writeIndex.compare_exchange_weak(oldWriteIndex,newWriteIndex);
            if(done) break;
          }
          return true;
        }
        --writeFlags[index];
      }
      return false;
    }

    // Pointer to notification latch used to realise a wait_any logic.
    std::shared_ptr<latch> notifyer;

    friend future_queue_base::id_t wait_any(std::list<std::reference_wrapper<future_queue_base>> listOfQueues);

  private:

    // return next available process-unique id_t
    static id_t get_next_id() {
      static std::atomic<size_t> nextId{0};
      ++nextId;
      return id_t(nextId);
    }

};

// A "lockfree" single-producer single-consumer queue of a fixed length which the receiver can wait on in case the queue
// is empty. This is similiar like using a lockfree queue of futures but has better performance. In addition the queue
// allows the sender to overwrite the last written element in case the queue is full. The receiver can also use the
// function wait_any() to wait until any of the given future_queues is not empty.
template<typename T>
class future_queue : public future_queue_base {

  public:

    // The length specifies how many objects the queue can contain at a time. Internally additional buffers will be
    // allocated. All buffers are allocated upon construction, so no dynamic memory allocation is required later.
    future_queue(size_t length)
    : future_queue_base(length),
      buffers(length+1)
    {}

    // Push object t to the queue. Returns true if successful and false if queue is full.
    bool push(T&& t) {
      size_t myIndex;
      if(!obtain_write_slot(myIndex)) return false;
      buffers[myIndex] = std::move(t);
      updateIds[myIndex] = get_next_update_id();
      latches[myIndex].count_down();
      // send notification if requested
      auto notify = std::atomic_load(&notifyer);
      if(notify) notify->count_down();
      return true;
    }

    // Push object t to the queue. If the queue is full, the last element will be overwritten and false will be
    // returned. If no data had to be overwritten, true is returned.
    // When using this function the queue must have a length of at least 2.
    // Note: when used in a multi-producer context the behaviour is undefined!
    bool push_overwrite(T&& t) {
      assert(nBuffers-1 > 1);
      bool ret = true;
      if(write_available() == 0) {
        if(latches[previousWriteIndex()].is_ready_and_reset()) {
          writeIndex = previousWriteIndex();
          ret = false;
        }
        else {
          // if the latch for the last written buffer is no longer readym it means the buffer has been read already. In
          // this case we should now have buffers available for writing.
          assert(write_available() > 0);
        }
      }
      buffers[writeIndex] = std::move(t);
      updateIds[writeIndex] = get_next_update_id();
      latches[writeIndex].count_down();
      writeIndex = nextWriteIndex();
      // send notification if requested
      auto notify = std::atomic_load(&notifyer);
      if(notify) notify->count_down();
      return ret;
    }

    // Pop object off the queue and store it in t. If no data is available, false is returned
    bool pop(T& t) {
      if(hasFrontOwnership || latches[readIndex].is_ready_and_reset()) {
        t = std::move(buffers[readIndex]);
        writeFlags[readIndex] = 0;
        readIndex = nextReadIndex();
        hasFrontOwnership = false;
        return true;
      }
      else {
        return false;
      }
    }

    // Pop object off the queue and discard it. If no data is available, false is returned
    bool pop() {
      T t;
      return pop(t);
    }

    // Pop object off the queue and store it in t. This function will block until data is available.
    void pop_wait(T& t) {
      if(!hasFrontOwnership) {
        latches[readIndex].wait_and_reset();
      }
      else {
        hasFrontOwnership = false;
      }
      t = std::move(buffers[readIndex]);
      writeFlags[readIndex] = 0;
      readIndex = nextReadIndex();
    }

    // Pop object off the queue and discard it. This function will block until data is available.
    void pop_wait() {
      T t;
      pop_wait(t);
    }

    // Obtain the front element of the queue without removing it. It is mandatory to make sure that data is available
    // in the queue by calling has_data() before calling this function.
    const T& front() const {
      assert(hasFrontOwnership);
      return buffers[readIndex];
    }

  private:

    // vector of buffers - allocation is done in the constructor
    std::vector<T> buffers;

};



// wait until any of the specified future_queue instances has new data
future_queue_base::id_t wait_any(std::list<std::reference_wrapper<future_queue_base>> listOfQueues) {

    // Create a latch in a shared pointer, so we can hand it on to the queues
    std::shared_ptr<latch> notifyer = std::make_shared<latch>();

    // Distribute the pointer to the latch to all queues - since we have variadic arguments we need to call a recursive
    // helper
    for(auto &queue : listOfQueues) std::atomic_store(&(queue.get().notifyer), notifyer);

    // Check if data is present in any of the queues. This check is repeated in a loop since spurious wakeups can occur.
    future_queue_base::id_t id(future_queue_base::id_t::nullid());
    size_t updateId = future_queue_base::get_max_update_id();
    while(true) {

      // search for data and store the id of the first update (if multiple updates present)
      for(auto &queue : listOfQueues) {
        if(queue.get().has_data()) {
          if(queue.get().get_update_id() < updateId) {
            updateId = queue.get().get_update_id();
            id = queue.get().get_id();
          }
        }
      }

      // if data has been found, terminate loop
      if(id != future_queue_base::id_t::nullid()) break;

      // If no data yet present, wait on the latch. Since we have locked it before, this will block
      // until one of the queues unlocks it
      notifyer->wait();
      // Note: suprious wakeups might occur due to race conditions in case push_overwrite()
    }

    // Distribute a nullptr to all queues so they no longer try to notify (wouldn't be a big problem but might hurt
    // performance)
    notifyer = std::shared_ptr<latch>(nullptr);
    for(auto &queue : listOfQueues) std::atomic_store(&(queue.get().notifyer), notifyer);

    // return the id of the queue which has the oldest update (determined in wait_any_helper_check_for_data())
    return id;
}
