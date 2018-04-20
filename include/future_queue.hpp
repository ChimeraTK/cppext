#ifndef FUTURE_QUEUE_HPP
#define FUTURE_QUEUE_HPP

#include <list>
#include <atomic>
#include <vector>
#include <cassert>
#include <map>

#include "semaphore.hpp"

template<typename T>
class future_queue;
// Base class for future_queue which does not depend on the template argument. For a description see future_queue.
// FIXME protect index handling against overruns of size_t!
class future_queue_base {

  public:

    // Number of push operations which can be performed before the queue is full.
    size_t write_available() const {
      size_t l_writeIndex = d->writeIndex;
      size_t l_readIndex = d->readIndex;
      if(l_writeIndex - l_readIndex < d->nBuffers-1) {
        return d->nBuffers - (l_writeIndex - l_readIndex) - 1;
      }
      else {
        return 0;
      }
    }

    // Number of pop operations which can be performed before the queue is empty. Note that the result can be inaccurate
    // in case the sender uses push_overwrite(). If a guarantee is required that a readable element is present before
    // accessing it through pop() or front(), use has_data().
    size_t read_available() {
      return d->readIndexMax - d->readIndex;
    }

    // Check for the presence of readable data in the queue.
    bool has_data() {
      if(d->hasFrontOwnership) return true;
      if(d->semaphores[d->readIndex%d->nBuffers].is_ready_and_reset()) {
        d->hasFrontOwnership = true;
        return true;
      }
      return false;
    }

    // return length of the queue
    size_t size() const {
      return d->nBuffers - 1;
    }

  protected:

    struct shared_state_base {

      shared_state_base(size_t length)
      : nBuffers(length+1),
        semaphores(length+1),
        writeIndex(0),
        readIndexMax(0),
        readIndex(0),
        hasFrontOwnership(false)
      {}

      // index used in wait_any to identify the queue
      size_t when_any_index;

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

    };

    future_queue_base(const std::shared_ptr<shared_state_base> &d_ptr_)
    : d_ptr(d_ptr_), d(d_ptr.get()) {}

    future_queue_base() : d(nullptr) {}

    // reserve next available write slot. Returns false if no free slot is available or true on success.
    bool obtain_write_slot(size_t &index) {
      index = d->writeIndex;
      while(true) {
        if(index >= d->readIndex+d->nBuffers - 1) return false;   // queue is full
        bool success = d->writeIndex.compare_exchange_weak(index, index+1);
        if(success) break;
      }
      return true;
    }

    // update readIndexMax after a write operation was completed
    void update_read_index_max() {
      size_t l_readIndex = d->readIndex;
      size_t l_writeIndex = d->writeIndex;
      size_t l_readIndexMax = d->readIndexMax;
      if(l_writeIndex >= l_readIndex+d->nBuffers) l_writeIndex = l_readIndex+d->nBuffers-1;
      size_t newReadIndexMax = l_readIndexMax;
      do {
        for(size_t index = l_readIndexMax; index <= l_writeIndex-1; ++index) {
          if(!d->semaphores[index % d->nBuffers].is_ready()) break;
          newReadIndexMax = index+1;
        }
        d->readIndexMax.compare_exchange_weak(l_readIndexMax, newReadIndexMax);
      } while(d->readIndexMax < newReadIndexMax);
    }

    // pointer to data used to allow sharing the queue (create multiple copies which all refer to the same queue).
    // for some reason, using the shared_ptr in the access is slower than a plain pointer, so we keep the shared_ptr
    // here only for the ownership and use in the implementation always the plain pointer (which points to the same
    // object).
    std::shared_ptr<shared_state_base> d_ptr;
    shared_state_base *d;

    template<typename ITERABLE_TYPE>
    friend std::shared_ptr<future_queue<size_t>> when_any(ITERABLE_TYPE listOfQueues);

    template<typename T>
    friend class future_queue;

};

// A "lockfree" single-producer single-consumer queue of a fixed length which the receiver can wait on in case the queue
// is empty. This is similiar like using a lockfree queue of futures but has better performance. In addition the queue
// allows the sender to overwrite the last written element in case the queue is full. The receiver can also use the
// function wait_any() to wait until any of the given future_queues is not empty.
template<typename T>
class no_when_any_future_queue : public future_queue_base {

  public:

    // Push object t to the queue. Returns true if successful and false if queue is full.
    bool push(T&& t) {
      size_t myIndex;
      if(!obtain_write_slot(myIndex)) return false;
      static_cast<no_when_any_shared_state*>(future_queue_base::d)->buffers[myIndex % d->nBuffers] = std::move(t);
      assert(!d->semaphores[myIndex % d->nBuffers].is_ready());
      d->semaphores[myIndex % d->nBuffers].unlock();
      update_read_index_max();
      return true;
    }
    bool push(const T& t) {
      return push(T(t));
    }

    // Push object t to the queue. If the queue is full, the last element will be overwritten and false will be
    // returned-> If no data had to be overwritten, true is returned->
    // When using this function the queue must have a length of at least 2.
    // Note: when used in a multi-producer context the behaviour is undefined!
    bool push_overwrite(T&& t) {
      assert(d->nBuffers-1 > 1);
      bool ret = true;
      if(write_available() == 0) {
        if(d->semaphores[(d->writeIndex-1)%d->nBuffers].is_ready_and_reset()) {
          ret = false;
        }
        else {
          // if the semaphore for the last written buffer is no longer ready it means the buffer has been read already. In
          // this case we should now have buffers available for writing.
          assert(write_available() > 0);
        }
        d->writeIndex--;
      }
      static_cast<no_when_any_shared_state*>(future_queue_base::d)->buffers[d->writeIndex%d->nBuffers] = std::move(t);
      d->writeIndex++;
      assert(!d->semaphores[(d->writeIndex-1)%d->nBuffers].is_ready());
      d->semaphores[(d->writeIndex-1)%d->nBuffers].unlock();
      d->readIndexMax = size_t(d->writeIndex);
      return ret;
    }

    // Pop object off the queue and store it in t. If no data is available, false is returned
    bool pop(T& t) {
      if( d->hasFrontOwnership || d->semaphores[d->readIndex%d->nBuffers].is_ready_and_reset() ) {
        t = std::move(static_cast<no_when_any_shared_state*>(future_queue_base::d)->buffers[d->readIndex%d->nBuffers]);
        assert(d->readIndex < d->writeIndex);
        d->readIndex++;
        d->hasFrontOwnership = false;
        return true;
      }
      else {
        return false;
      }
    }

    // Pop object off the queue and store it in t. This function will block until data is available.
    void pop_wait(T& t) {
      if(!d->hasFrontOwnership) {
        d->semaphores[d->readIndex%d->nBuffers].wait_and_reset();
      }
      else {
        d->hasFrontOwnership = false;
      }
      t = std::move(static_cast<no_when_any_shared_state*>(future_queue_base::d)->buffers[d->readIndex%d->nBuffers]);
      assert(d->readIndex < d->writeIndex);
      d->readIndex++;
    }

    // Obtain the front element of the queue without removing it. It is mandatory to make sure that data is available
    // in the queue by calling has_data() before calling this function.
    const T& front() const {
      assert(d->hasFrontOwnership);
      return static_cast<no_when_any_shared_state*>(future_queue_base::d)->buffers[d->readIndex%d->nBuffers];
    }

  protected:

    struct no_when_any_shared_state : shared_state_base {
      no_when_any_shared_state(size_t length)
      : shared_state_base(length), buffers(length+1)
      {}

      // vector of buffers - allocation is done in the constructor
      std::vector<T> buffers;
    };

    no_when_any_future_queue(const std::shared_ptr<no_when_any_shared_state> &d_ptr_)
    : future_queue_base(d_ptr_)
    {}

  public:

    no_when_any_future_queue() {}

    friend no_when_any_future_queue<T> atomic_load(const no_when_any_future_queue<T>* p) {
      no_when_any_future_queue<T> q;
      q.d_ptr = std::atomic_load(&(p->d_ptr));
      q.d = q.d_ptr.get();
      return q;
    }

    friend void atomic_store(no_when_any_future_queue<T>* p, no_when_any_future_queue<T> r) {
      atomic_store(&(p->d_ptr), r->d_ptr);
    }
};

template<typename T>
class future_queue : public no_when_any_future_queue<T> {

  protected:

    struct shared_state : no_when_any_future_queue<T>::no_when_any_shared_state {
      shared_state(size_t length)
      : no_when_any_future_queue<T>::no_when_any_shared_state(length),
        notifyerQueue_previousData(0)
      {}

      // Notification queue used to realise a wait_any logic. This queue is shared between all queues participating in
      // the same when_any.
      no_when_any_future_queue<size_t> notifyerQueue;

      // counter for the number of elements in the queue before when_any has added the notifyerQueue
      std::atomic<size_t> notifyerQueue_previousData;
    };

  public:

    // The length specifies how many objects the queue can contain at a time. Internally additional buffers will be
    // allocated-> All buffers are allocated upon construction, so no dynamic memory allocation is required later.
    future_queue(size_t length)
    : no_when_any_future_queue<T>(std::make_shared<shared_state>(length))
    {}

    // The default constructor creates only a place holder which can later be assigned with a properly constructed
    // queue.
    future_queue() {}

    // Assignment operator
    future_queue& operator=(const future_queue &other) = default;

    bool push(T&& t) {
      auto r = no_when_any_future_queue<T>::push(std::move(t));
      // send notification if requested
      if(r) {
        auto notify = atomic_load(&static_cast<shared_state*>(future_queue_base::d)->notifyerQueue);
        if(notify.d_ptr) {
          bool nret = notify.push(static_cast<shared_state*>(future_queue_base::d)->when_any_index);
          (void)nret;
          assert(nret == true);
        }
        else {
          static_cast<shared_state*>(future_queue_base::d)->notifyerQueue_previousData++;
        }
      }
      return r;
    }
    bool push(const T& t) {
      return push(T(t));
    }

    bool push_overwrite(T&& t) {
      auto r = no_when_any_future_queue<T>::push_overwrite(std::move(t));
      // send notification if requested and if data wasn't overwritten
      if(r) {
        auto notify = atomic_load(&static_cast<shared_state*>(future_queue_base::d)->notifyerQueue);
        if(notify.d_ptr) {
          bool nret = notify.push(static_cast<shared_state*>(future_queue_base::d)->when_any_index);
          (void)nret;
          assert(nret == true);
        }
        else {
          static_cast<shared_state*>(future_queue_base::d)->notifyerQueue_previousData++;
        }
      }
      return r;
    }
    bool push_overwrite(const T& t) {
      return push_overwrite(T(t));
    }

    bool pop(T& t) {
      auto r = no_when_any_future_queue<T>::pop(t);
      static_cast<shared_state*>(future_queue_base::d)->notifyerQueue_previousData--;
      return r;
    }

    void pop_wait(T& t) {
      no_when_any_future_queue<T>::pop_wait(t);
      static_cast<shared_state*>(future_queue_base::d)->notifyerQueue_previousData--;
    }

    // Pop object off the queue and discard it. If no data is available, false is returned
    bool pop() {
      T t;
      return pop(t);
    }

    // Pop object off the queue and discard it. This function will block until data is available.
    void pop_wait() {
      T t;
      pop_wait(t);
    }

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
// therefore recommended to use pop_wait() to retrieve the values from the data queues if push_overwrite() is used->
// Otherwise failed pop() have to be retried until the data is received->
//
// If data is already available in the queues before calling when_any, the appropriate number of notifications are
// placed in the notifyer queue in arbitrary order.
template<typename ITERABLE_TYPE>
future_queue<size_t> when_any(ITERABLE_TYPE listOfQueues) {

    // Add lengthes of all queues - this will be the length of the notification queue
    size_t summedLength = 0;
    for(auto &queue : listOfQueues) summedLength += queue.size();

    // Create a notification queue in a shared pointer, so we can hand it on to the queues
    future_queue<size_t> notifyerQueue(summedLength);
    no_when_any_future_queue<size_t> no_when_any_notifyerQueue(std::static_pointer_cast<no_when_any_future_queue<size_t>::no_when_any_shared_state>(notifyerQueue.d_ptr));

    // Distribute the pointer to the notification queue to all participating queues
    size_t index = 0;
    for(auto &queue : listOfQueues) {
      atomic_store(&(queue.d->notifyerQueue), notifyerQueue);
      // at this point, queue.notifyerQueue_previousData will no longer be modified by the sender side
      size_t nPreviousValues = queue.d->notifyerQueue_previousData;
      queue.d->when_any_index = index;
      for(size_t i=0; i<nPreviousValues; ++i) notifyerQueue.push(index);
      ++index;
    }

    return notifyerQueue;
}

#endif // FUTURE_QUEUE_HPP
