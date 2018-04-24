#ifndef FUTURE_QUEUE_HPP
#define FUTURE_QUEUE_HPP

#include <list>
#include <atomic>
#include <vector>
#include <cassert>
#include <map>

#include "semaphore.hpp"

/*********************************************************************************************************************/

namespace detail {
  struct shared_state_base;

  template<typename T>
  struct shared_state;
}

template<typename T, typename FEATURES>
class future_queue;

/*********************************************************************************************************************/

namespace detail {

  // Base class for future_queue which does not depend on the template argument. For a description see future_queue.
  // FIXME protect index handling against overruns of size_t!
  class future_queue_base {

    public:

      // Number of push operations which can be performed before the queue is full.
      size_t write_available() const;

      // Number of pop operations which can be performed before the queue is empty. Note that the result can be inaccurate
      // in case the sender uses push_overwrite(). If a guarantee is required that a readable element is present before
      // accessing it through pop() or front(), use has_data().
      size_t read_available();

      // Check for the presence of readable data in the queue.
      bool has_data();

      // return length of the queue
      size_t size() const;

    protected:

      future_queue_base(const std::shared_ptr<shared_state_base> &d_ptr_);

      future_queue_base();

      // reserve next available write slot. Returns false if no free slot is available or true on success.
      bool obtain_write_slot(size_t &index);

      // update readIndexMax after a write operation was completed
      void update_read_index_max();

      // pointer to data used to allow sharing the queue (create multiple copies which all refer to the same queue).
      std::shared_ptr<shared_state_base> d;

      template<typename T, typename FEATURES>
      friend class ::future_queue;

  };

} // namespace detail

/*********************************************************************************************************************/

// feature tag for future_queue: use std::move to store and retreive data to/from the queue
class MOVE_DATA {};

// feature tag for future_queue: use std::swap to store and retreive data to/from the queue
class SWAP_DATA {};

/*********************************************************************************************************************/

// A lockfree multi-producer single-consumer queue of a fixed length which the receiver can wait on in case the queue
// is empty. This is similiar like using a lockfree queue of futures but has better performance. In addition the queue
// allows the sender to overwrite the last written element in case the queue is full. The receiver can also use the
// function when_any() to get notified when any of the given future_queues is not empty.
template<typename T, typename FEATURES=MOVE_DATA>
class future_queue : public detail::future_queue_base {

  public:

    // The length specifies how many objects the queue can contain at a time. Internally additional buffers will be
    // allocated-> All buffers are allocated upon construction, so no dynamic memory allocation is required later.
    future_queue(size_t length);

    // The default constructor creates only a place holder which can later be assigned with a properly constructed
    // queue.
    future_queue();

    // Assignment operator
    future_queue& operator=(const future_queue &other) = default;

    // Push object t to the queue. Returns true if successful and false if queue is full.
    bool push(T&& t);
    bool push(const T& t);

    // Push object t to the queue. If the queue is full, the last element will be overwritten and false will be
    // returned-> If no data had to be overwritten, true is returned->
    // When using this function the queue must have a length of at least 2.
    // Note: when used in a multi-producer context the behaviour is undefined!
    bool push_overwrite(T&& t);
    bool push_overwrite(const T& t);

    // Pop object off the queue and store it in t. If no data is available, false is returned
    bool pop(T& t);
    bool pop();

    // Pop object off the queue and store it in t. This function will block until data is available.
    void pop_wait(T& t);
    void pop_wait();

    // Obtain the front element of the queue without removing it. It is mandatory to make sure that data is available
    // in the queue by calling has_data() before calling this function.
    const T& front() const;

    friend future_queue<T,FEATURES> atomic_load(const future_queue<T,FEATURES>* p) {
      future_queue<T,FEATURES> q;
      q.d = std::atomic_load(&(p->d));
      return q;
    }

    friend void atomic_store(future_queue<T,FEATURES>* p, future_queue<T,FEATURES> r) {
      atomic_store(&(p->d), r.d);
    }

    template<typename ITERABLE_TYPE>
    friend future_queue<size_t> when_any(ITERABLE_TYPE listOfQueues);

    typedef T value_type;

};

/*********************************************************************************************************************/

namespace detail {

  // Internal base class for holding the data which is shared between multiple instances of the same queue. The base
  // class does not depend on the data type and is used by future_queue_base.
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

  // Internal class for holding the data which is shared between multiple instances of the same queue. This class is
  // depeding on the data type and is used by the future_queue class.
  template<typename T>
  struct shared_state : shared_state_base {
    shared_state(size_t length)
    : shared_state_base(length), buffers(length+1), notifyerQueue_previousData(0)
    {}

    // vector of buffers - allocation is done in the constructor
    std::vector<T> buffers;

    // Notification queue used to realise a wait_any logic. This queue is shared between all queues participating in
    // the same when_any.
    future_queue<size_t> notifyerQueue;

    // counter for the number of elements in the queue before when_any has added the notifyerQueue
    std::atomic<size_t> notifyerQueue_previousData;

  };

} // namespace detail

/*********************************************************************************************************************/

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

    // Distribute the pointer to the notification queue to all participating queues
    size_t index = 0;
    for(auto &queue : listOfQueues) {
      typedef detail::shared_state<typename std::remove_reference<decltype(queue)>::type::value_type> shared_state_type;
      auto *shared_state = static_cast<shared_state_type*>(queue.d.get());
      atomic_store(&(shared_state->notifyerQueue), notifyerQueue);
      // at this point, queue.notifyerQueue_previousData will no longer be modified by the sender side
      size_t nPreviousValues = shared_state->notifyerQueue_previousData;
      queue.d->when_any_index = index;
      for(size_t i=0; i<nPreviousValues; ++i) notifyerQueue.push(index);
      ++index;
    }

    return notifyerQueue;
}

/*********************************************************************************************************************/

namespace detail {

  // helper function to realise the data assignment depending on the selected FEATURES tags. The last dummy argument is
  // just used to realise overloads for the different tags (as C++ does not know partial template specialisations for
  // functions).
  template<typename T>
  void data_assign(T& a, T&& b, MOVE_DATA) {
    // in order not to depend on the move assignment operator, which might not always be available, we perform an
    // in-place destruction followed by an in-place move construction.
    a.~T();
    new (&a) T(std::move(b));
  }

  template<typename T>
  void data_assign(T& a, T&& b, SWAP_DATA) {
    std::swap(a,b);
  }

} // namespace detail

/*********************************************************************************************************************/

namespace detail {

  size_t future_queue_base::write_available() const {
    size_t l_writeIndex = d->writeIndex;
    size_t l_readIndex = d->readIndex;
    if(l_writeIndex - l_readIndex < d->nBuffers-1) {
      return d->nBuffers - (l_writeIndex - l_readIndex) - 1;
    }
    else {
      return 0;
    }
  }

  size_t future_queue_base::read_available() {
    return d->readIndexMax - d->readIndex;
  }

  bool future_queue_base::has_data() {
    if(d->hasFrontOwnership) return true;
    if(d->semaphores[d->readIndex%d->nBuffers].is_ready_and_reset()) {
      d->hasFrontOwnership = true;
      return true;
    }
    return false;
  }

  size_t future_queue_base::size() const {
    return d->nBuffers - 1;
  }

  future_queue_base::future_queue_base(const std::shared_ptr<shared_state_base> &d_ptr_)
  : d(d_ptr_) {}

  future_queue_base::future_queue_base() : d(nullptr) {}

  bool future_queue_base::obtain_write_slot(size_t &index) {
    index = d->writeIndex;
    while(true) {
      if(index >= d->readIndex+d->nBuffers - 1) return false;   // queue is full
      bool success = d->writeIndex.compare_exchange_weak(index, index+1);
      if(success) break;
    }
    return true;
  }

  void future_queue_base::update_read_index_max() {
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

} // namespace detail

/*********************************************************************************************************************/

template<typename T, typename FEATURES>
future_queue<T,FEATURES>::future_queue(size_t length)
: future_queue_base(std::make_shared<detail::shared_state<T>>(length))
{}

template<typename T, typename FEATURES>
future_queue<T,FEATURES>::future_queue() {}

template<typename T, typename FEATURES>
bool future_queue<T,FEATURES>::push(T&& t) {
  size_t myIndex;
  if(!obtain_write_slot(myIndex)) return false;
  detail::data_assign(
    static_cast<detail::shared_state<T>*>(future_queue_base::d.get())->buffers[myIndex % d->nBuffers],
    std::move(t), FEATURES()
  );
  assert(!d->semaphores[myIndex % d->nBuffers].is_ready());
  d->semaphores[myIndex % d->nBuffers].unlock();
  update_read_index_max();
  // send notification if requested
  auto notify = atomic_load(&static_cast<detail::shared_state<T>*>(future_queue_base::d.get())->notifyerQueue);
  if(notify.d) {
    bool nret = notify.push(static_cast<detail::shared_state<T>*>(future_queue_base::d.get())->when_any_index);
    (void)nret;
    assert(nret == true);
  }
  else {
    static_cast<detail::shared_state<T>*>(future_queue_base::d.get())->notifyerQueue_previousData++;
  }
  return true;
}

template<typename T, typename FEATURES>
bool future_queue<T,FEATURES>::push(const T& t) {
  return push(T(t));
}

template<typename T, typename FEATURES>
bool future_queue<T,FEATURES>::push_overwrite(T&& t) {
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
  detail::data_assign(
    static_cast<detail::shared_state<T>*>(future_queue_base::d.get())->buffers[d->writeIndex%d->nBuffers],
    std::move(t), FEATURES()
  );
  d->writeIndex++;
  assert(!d->semaphores[(d->writeIndex-1)%d->nBuffers].is_ready());
  d->semaphores[(d->writeIndex-1)%d->nBuffers].unlock();
  d->readIndexMax = size_t(d->writeIndex);

  // send notification if requested and if data wasn't overwritten
  if(ret) {
    auto notify = atomic_load(&static_cast<detail::shared_state<T>*>(future_queue_base::d.get())->notifyerQueue);
    if(notify.d) {
      bool nret = notify.push(static_cast<detail::shared_state<T>*>(future_queue_base::d.get())->when_any_index);
      (void)nret;
      assert(nret == true);
    }
    else {
      static_cast<detail::shared_state<T>*>(future_queue_base::d.get())->notifyerQueue_previousData++;
    }
  }
  return ret;
}

template<typename T, typename FEATURES>
bool future_queue<T,FEATURES>::push_overwrite(const T& t) {
  return push_overwrite(T(t));
}

template<typename T, typename FEATURES>
bool future_queue<T,FEATURES>::pop(T& t) {
  if( d->hasFrontOwnership || d->semaphores[d->readIndex%d->nBuffers].is_ready_and_reset() ) {
    detail::data_assign(
      t,
      std::move(static_cast<detail::shared_state<T>*>(future_queue_base::d.get())->buffers[d->readIndex%d->nBuffers]),
      FEATURES()
    );
    assert(d->readIndex < d->writeIndex);
    d->readIndex++;
    d->hasFrontOwnership = false;
    static_cast<detail::shared_state<T>*>(future_queue_base::d.get())->notifyerQueue_previousData--;
    return true;
  }
  else {
    return false;
  }
}

template<typename T, typename FEATURES>
bool future_queue<T,FEATURES>::pop() {
  if( d->hasFrontOwnership || d->semaphores[d->readIndex%d->nBuffers].is_ready_and_reset() ) {
    assert(d->readIndex < d->writeIndex);
    d->readIndex++;
    d->hasFrontOwnership = false;
    static_cast<detail::shared_state<T>*>(future_queue_base::d.get())->notifyerQueue_previousData--;
    return true;
  }
  else {
    return false;
  }
}

template<typename T, typename FEATURES>
void future_queue<T,FEATURES>::pop_wait(T& t) {
  if(!d->hasFrontOwnership) {
    d->semaphores[d->readIndex%d->nBuffers].wait_and_reset();
  }
  else {
    d->hasFrontOwnership = false;
  }
  detail::data_assign(
    t,
    std::move(static_cast<detail::shared_state<T>*>(future_queue_base::d.get())->buffers[d->readIndex%d->nBuffers]),
    FEATURES()
  );
  assert(d->readIndex < d->writeIndex);
  d->readIndex++;
  static_cast<detail::shared_state<T>*>(future_queue_base::d.get())->notifyerQueue_previousData--;
}

template<typename T, typename FEATURES>
void future_queue<T,FEATURES>::pop_wait() {
  if(!d->hasFrontOwnership) {
    d->semaphores[d->readIndex%d->nBuffers].wait_and_reset();
  }
  else {
    d->hasFrontOwnership = false;
  }
  assert(d->readIndex < d->writeIndex);
  d->readIndex++;
  static_cast<detail::shared_state<T>*>(future_queue_base::d.get())->notifyerQueue_previousData--;
}

template<typename T, typename FEATURES>
const T& future_queue<T,FEATURES>::front() const {
  assert(d->hasFrontOwnership);
  return static_cast<detail::shared_state<T>*>(future_queue_base::d.get())->buffers[d->readIndex%d->nBuffers];
}

#endif // FUTURE_QUEUE_HPP
