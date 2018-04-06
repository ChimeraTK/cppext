#include <thread>
#include <iostream>
#include <atomic>
#include <vector>
#include <assert.h>
#include <unistd.h>

#include "semaphore.hpp"

template<typename T>
class future_queue;

// Base class for future_queue which does not depend on the template argument. For a description see future_queue.
// FIXME protect index handling against overruns of size_t!
class future_queue_base {

  public:

    // Number of push operations which can be performed before the queue is full.
    size_t write_available() const {
      size_t l_writeIndex = writeIndex;
      size_t l_readIndex = readIndex;
      if(l_writeIndex - l_readIndex < nBuffers-1) {
        return nBuffers - (l_writeIndex - l_readIndex) - 1;
      }
      else {
        return 0;
      }
    }

    // Number of pop operations which can be performed before the queue is empty. Note that the result can be inaccurate
    // in case the sender uses push_overwrite(). If a guarantee is required that a readable element is present before
    // accessing it through pop() or front(), use has_data().
    size_t read_available() {
      return readIndexMax - readIndex;
    }

    // Check for the presence of readable data in the queue.
    bool has_data() {
      if(hasFrontOwnership) return true;
      if(semaphores[readIndex%nBuffers].is_ready_and_reset()) {
        hasFrontOwnership = true;
        return true;
      }
      return false;
    }

    // Class for the process-unique id_t, to prevent exposing implementation details of the id_t to the public interface
    class id_t {
      public:
        id_t(const id_t &other) : _id(other._id) {}
        id_t(id_t &&other) : _id(other._id) {}
        id_t() : _id(0) {}
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
    id_t get_id() const {
      return id;
    }

    // return length of the queue
    size_t size() const {
      return nBuffers - 1;
    }

  //protected:      // FIXME make protected again - requires change of testObtainWriteSlot

    future_queue_base(size_t length)
    : id(get_next_id()),
      nBuffers(length+1),
      semaphores(length+1),
      writeIndex(0),
      readIndexMax(0),
      readIndex(0),
      hasFrontOwnership(false)
    {}

    // process-unique id_t of this queue
    id_t id;

    // the number of buffers we have allocated
    size_t nBuffers;

    // vector of semaphores corresponding to the buffers which allows the receiver to wait for new data
    std::vector<semaphore> semaphores;

    // index of the element which will be next written
    std::atomic<size_t> writeIndex;

    // maximum index which the receiver is currently allowed to read (after checking it semaphore). Often equal to
    // writeIndex, unless write operations are currently in progress
    std::atomic<size_t> readIndexMax;

    // index of the element which will be next read
    std::atomic<size_t> readIndex;

    // Flag if the receiver has already ownership over the front element. This flag may only be used by the receiver.
    bool hasFrontOwnership;

    // reserve next available write slot. Returns false if no free slot is available or true on success.
    bool obtain_write_slot(size_t &index) {
      index = writeIndex;
      while(true) {
        if(index >= readIndex+nBuffers - 1) return false;   // queue is full
        bool success = writeIndex.compare_exchange_weak(index, index+1);
        if(success) break;
      }
      return true;
    }

    // update readIndexMax after a write operation was completed
    void update_read_index_max() {
      size_t l_readIndex = readIndex;
      size_t l_writeIndex = writeIndex;
      size_t l_readIndexMax = readIndexMax;
      if(l_writeIndex >= l_readIndex+nBuffers) l_writeIndex = l_readIndex+nBuffers-1;
      size_t newReadIndexMax = l_readIndexMax;
      do {
        for(size_t index = l_readIndexMax; index <= l_writeIndex-1; ++index) {
          if(!semaphores[index % nBuffers].is_ready()) break;
          newReadIndexMax = index+1;
        }
        readIndexMax.compare_exchange_weak(l_readIndexMax, newReadIndexMax);
      } while(readIndexMax < newReadIndexMax);
    }

    // Pointer to notification queue used to realise a wait_any logic.
    std::shared_ptr<future_queue<id_t>> notifyerQueue;

    friend std::shared_ptr<future_queue<future_queue_base::id_t>> when_any(std::list<std::reference_wrapper<future_queue_base>> listOfQueues);

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
      buffers[myIndex % nBuffers] = std::move(t);
      assert(!semaphores[myIndex % nBuffers].is_ready());
      semaphores[myIndex % nBuffers].unlock();
      update_read_index_max();
      // send notification if requested
      auto notify = std::atomic_load(&notifyerQueue);
      if(notify) {
        bool nret = notify->push(get_id());
        (void)nret;
        assert(nret == true);
      }
      return true;
    }
    bool push(const T& t) {
      return push(T(t));
    }

    // Push object t to the queue. If the queue is full, the last element will be overwritten and false will be
    // returned. If no data had to be overwritten, true is returned.
    // When using this function the queue must have a length of at least 2.
    // Note: when used in a multi-producer context the behaviour is undefined!
    bool push_overwrite(T&& t) {
      assert(nBuffers-1 > 1);
      bool ret = true;
      if(write_available() == 0) {
        if(semaphores[(writeIndex-1)%nBuffers].is_ready_and_reset()) {
          ret = false;
        }
        else {
          // if the semaphore for the last written buffer is no longer ready it means the buffer has been read already. In
          // this case we should now have buffers available for writing.
          assert(write_available() > 0);
        }
        writeIndex--;
      }
      buffers[writeIndex%nBuffers] = std::move(t);
      writeIndex++;
      assert(!semaphores[(writeIndex-1)%nBuffers].is_ready());
      semaphores[(writeIndex-1)%nBuffers].unlock();
      readIndexMax = size_t(writeIndex);
      // send notification if requested and if data wasn't overwritten
      if(ret) {
        auto notify = std::atomic_load(&notifyerQueue);
        if(notify) notify->push(get_id());
      }
      return ret;
    }
    bool push_overwrite(const T& t) {
      return push_overwrite(T(t));
    }

    // Pop object off the queue and store it in t. If no data is available, false is returned
    bool pop(T& t) {
      if( hasFrontOwnership || semaphores[readIndex%nBuffers].is_ready_and_reset() ) {
        t = std::move(buffers[readIndex%nBuffers]);
        assert(readIndex < writeIndex);
        readIndex++;
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
        semaphores[readIndex%nBuffers].wait_and_reset();
      }
      else {
        hasFrontOwnership = false;
      }
      t = std::move(buffers[readIndex%nBuffers]);
      assert(readIndex < writeIndex);
      readIndex++;
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
      return buffers[readIndex%nBuffers];
    }

  private:

    // vector of buffers - allocation is done in the constructor
    std::vector<T> buffers;

};



// Return a future_queue which will receive the future_queue_base::id_t of each queue in listOfQueues when the
// respective queue has new data available for reading. This way the returned queue can be used to get notified about
// each data written to any of the queues. The order of the ids in this queue is guaranteed to be in the same order
// the data has been written to the queues. If the same queue gets written to multiple times its id will be present in
// the returned queue the same number of times.
//
// Behaviour is unspecified if, after the call to when_any, data is popped from one of the queues in the listOfQueues
// without retreiving its id previously from the returned queue. Behaviour is also unspecified if the same queue is
// passed to different calls to this function.
//
// If push_overwrite() is used on one of the queues in listOfQueues, the notifications received through the returned
// queue might be in a different order (i.e. when data is overwritten, the corresponding queue id is not moved to
// the correct place later in the notfication queue). Also, a notification for a value written to a queue with
// push_overwrite() might appear in the notification queue before the value can be retrieve from the data queue. It is
// therefore recommended to use pop_wait() to retrieve the values from the data queues if push_overwrite() is used.
// Otherwise failed pop() have to be retried until the data is received.
std::shared_ptr<future_queue<future_queue_base::id_t>> when_any(std::list<std::reference_wrapper<future_queue_base>> listOfQueues) {

    // Add lengthes of all queues - this will be the length of the notification queue
    size_t summedLength = 0;
    for(auto &queue : listOfQueues) summedLength += queue.get().size();

    // Create a notification queue in a shared pointer, so we can hand it on to the queues
    auto notifyerQueue = std::make_shared<future_queue<future_queue_base::id_t>>(summedLength);

    // Distribute the pointer to the notification queue to all participating queues
    for(auto &queue : listOfQueues) std::atomic_store(&(queue.get().notifyerQueue), notifyerQueue);

    return notifyerQueue;
}
