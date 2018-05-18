#ifndef FUTURE_QUEUE_HPP
#define FUTURE_QUEUE_HPP

#include <atomic>
#include <vector>
#include <cassert>
#include <future>     // just for std::launch

#include "semaphore.hpp"

namespace cppext {

/*********************************************************************************************************************/

  /** Feature tag for future_queue: use std::move to store and retreive data to/from the queue */
  class MOVE_DATA {};

  /** Feature tag for future_queue: use std::swap to store and retreive data to/from the queue */
  class SWAP_DATA {};

  /*********************************************************************************************************************/

  namespace detail {
    struct shared_state_base;

    template<typename T>
    struct shared_state;
  }

  template<typename T, typename FEATURES>
  class future_queue;

  /*********************************************************************************************************************/

  /** Type-independent base class for future_queue which does not depend on the template argument.
   *  For a description see future_queue. */
  class future_queue_base {

    public:

      /** Number of push operations which can be performed before the queue is full. Note that the result may be
       *  inacurate e.g. in multi-producer contexts. */
      size_t write_available() const;

      /** Number of pop operations which can be performed before the queue is empty. Note that the result can be inaccurate
       *  in case the sender uses push_overwrite(). If a guarantee is required that a readable element is present before
       * accessing it through pop() or front(), use empty(). */
      size_t read_available() const;

      /** Push an exception pointer (inplace of a value) into the queue. The exception gets thrown by
       *  pop()/pop_wait()/front() when the receiver reads the corresponding queue element. */
      bool push_exception(std::exception_ptr exception);

      /** Check if there is currently no data on the queue. If the queue contains data (i.e. true will be returned),
       *  the function will guarantee that this data can be accessed later e.g. thorugh front() or pop(). This
       *  guarantee holds even if the sender uses pop_overwrite(). */
      bool empty();

      /** Wait until the queue is not empty. This function guarantees similar like empty() that after this call data
       *  can be accessed e.g. through front() or pop(). */
      void wait();

      /** return length of the queue */
      size_t size() const;

    protected:

      future_queue_base(const std::shared_ptr<detail::shared_state_base> &d_ptr_);

      future_queue_base();

      /** reserve next available write slot. Returns false if no free slot is available or true on success. */
      bool obtain_write_slot(size_t &index);

      /** update readIndexMax after a write operation was completed */
      void update_read_index_max();

      /** pointer to data used to allow sharing the queue (create multiple copies which all refer to the same queue). */
      std::shared_ptr<detail::shared_state_base> d;

      template<typename T, typename FEATURES>
      friend class ::cppext::future_queue;

      template<typename ITERATOR_TYPE>
      friend future_queue<size_t,MOVE_DATA> when_any(ITERATOR_TYPE begin, ITERATOR_TYPE end);

      template<typename ITERATOR_TYPE>
      friend future_queue<void,MOVE_DATA> when_all(ITERATOR_TYPE begin, ITERATOR_TYPE end);

  };

  /*********************************************************************************************************************/

  /**
   *  A lockfree multi-producer single-consumer queue of a fixed length which the receiver can wait on in case the queue
   *  is empty. This is similiar like using a lockfree queue of futures but has better performance. In addition the queue
   *  allows the sender to overwrite the last written element in case the queue is full. The receiver can also use the
   *  function when_any() to get notified when any of the given future_queues is not empty.
   *
   *  The template parameter T specifies the type of the user data stored in the queue. The optional second template
   *  parameter takes one of the feature tags. Currently two options are supported:
   *
   *   - MOVE_DATA (default): Type T must have a move constructor. To place objects on the queue and to retrieve them
   *                          from the queue, a move operation is performed.
   *   - SWAP_DATA:           The function std::swap() must be overloaded for the type T. When placing objects on the
   *                          queue, std::swap() is called to exchange the new object with the object currently on the
   *                          internal queue buffer. This allows avoiding unnecessary memory allocations e.g. when
   *                          storing std::vector on the queue, especially if all vectors have the same size.
   *
   *  In both cases, T must be default constructible. Upon creation of the queue all internal buffers will be filled
   *  with default constructed elements.
   */
  template<typename T, typename FEATURES=MOVE_DATA>
  class future_queue : public future_queue_base {

    public:

      /** The length specifies how many objects the queue can contain at a time. Internally additional buffers will be
       *  allocated-> All buffers are allocated upon construction, so no dynamic memory allocation is required later. */
      future_queue(size_t length);

      /** The default constructor creates only a place holder which can later be assigned with a properly constructed
       *  queue. */
      future_queue();

      /** Copy constructor: After copying the object both *this and the other object will refer to the same queue. */
      future_queue(const future_queue &other) = default;

      /** Copy assignment operator: After the assignment both *this and the other object will refer to the same
       *  queue. */
      future_queue& operator=(const future_queue &other) = default;

      /** Push object t to the queue. Returns true if successful and false if queue is full. */
      template<typename U=T, typename std::enable_if< std::is_same<T,U>::value && !std::is_same<U, void>::value, int >::type = 0>
      bool push(U&& t);
      template<typename U=T, typename std::enable_if< !std::is_same<U, void>::value, int >::type = 0>
      bool push(const U& t);

      /** This version of push() is valid only for T=void */
      bool push(void);

      /** Push object t to the queue. If the queue is full, the last element will be overwritten and false will be
       *  returned. If no data had to be overwritten, true is returned.
       *
       *  When using this function, the queue must have a length of at least 2.
       *
       *  Note: when used in a multi-producer context the behaviour is undefined! */
      template<typename U=T, typename std::enable_if< std::is_same<T,U>::value && !std::is_same<U, void>::value, int >::type = 0>
      bool push_overwrite(U&& t);

      template<typename U=T, typename std::enable_if< !std::is_same<U, void>::value, int >::type = 0>
      bool push_overwrite(const U& t);

      /** This version of push_overwrite() is valid only for T=void */
      bool push_overwrite();

      /** Pop object off the queue and store it in t. If no data is available, false is returned */
      template<typename U=T, typename std::enable_if< std::is_same<T,U>::value && !std::is_same<U, void>::value, int >::type = 0>
      bool pop(U& t);

      bool pop();

      /** Pop object off the queue and store it in t. This function will block until data is available. */
      template<typename U=T, typename std::enable_if< std::is_same<T,U>::value && !std::is_same<U, void>::value, int >::type = 0>
      void pop_wait(U& t);

      void pop_wait();

      /** Obtain the front element of the queue without removing it. It is mandatory to make sure that data is available
       * in the queue by calling empty() before calling this function. */
      template<typename U=T, typename std::enable_if< std::is_same<T,U>::value && !std::is_same<U, void>::value, int >::type = 0>
      const U& front() const;

      /** Add continuation: Whenever there is a new element in the queue, process it with the callable and put the result
       *  into a new queue. The new queue will be returned by this function.
       *
       *  The signature of the callable must be "T2(T)", i.e. it has a single argument of the value type T of the
       *  queue then() is called on, and the return type matches the value type of the returned queue.
       *
       *  Two different launch policies can be selected:
       *   - std::launch::async will launch a new thread and trigger data processing asynchronously in the background.
       *     Each value will be processed in the order they are pushed to the queue and in the same thread.
       *   - std::launch::deferred will defer data processing until the data is accessed on the resulting queue.
       *     Checking the presence through empty() is already counted an access. If the same data is accessed multiple
       *     times (e.g. by calling front() several times), the callable is only executed once.
       *  If neither std::launch::async (which is the default) nor std::launch::deferred is specified, the behaviour is
       *  undefined. */
      template<typename T2, typename FEATURES2=MOVE_DATA, typename CALLABLE>
      future_queue<T2,FEATURES2> then(CALLABLE callable, std::launch policy = std::launch::async);

      friend future_queue<T,FEATURES> atomic_load(const future_queue<T,FEATURES>* p) {
        future_queue<T,FEATURES> q;
        q.d = std::atomic_load(&(p->d));
        return q;
      }

      friend void atomic_store(future_queue<T,FEATURES>* p, future_queue<T,FEATURES> r) {
        atomic_store(&(p->d), r.d);
      }

      typedef T value_type;

  };

  /*********************************************************************************************************************/

  namespace detail {

    /** Internal base class for holding the data which is shared between multiple instances of the same queue. The base
     *  class does not depend on the data type and is used by future_queue_base. */
    struct shared_state_base {

      shared_state_base(size_t length)
      : nBuffers(length+1),
        semaphores(length+1),
        exceptions(length+1),
        writeIndex(0),
        readIndexMax(0),
        readIndex(0),
        hasFrontOwnership(false),
        notifyerQueue_previousData(0)
      {}

      /** index used in wait_any to identify the queue */
      size_t when_any_index;

      /** the number of buffers we have allocated */
      size_t nBuffers;

      /** vector of semaphores corresponding to the buffers which allows the receiver to wait for new data */
      std::vector<semaphore> semaphores;

      /** vector of exception pointers, can be set instead of values through push_exception() */
      std::vector<std::exception_ptr> exceptions;

      /** index of the element which will be next written
       *  @todo FIXME protect handling of all indices against overruns of size_t! */
      std::atomic<size_t> writeIndex;

      /** maximum index which the receiver is currently allowed to read (after checking it semaphore). Often equal to
       *  writeIndex, unless write operations are currently in progress */
      std::atomic<size_t> readIndexMax;

      /** index of the element which will be next read */
      std::atomic<size_t> readIndex;

      /** Flag if the receiver has already ownership over the front element. This flag may only be used by the
       *  receiver. */
      bool hasFrontOwnership;

      /** Flag whether this future_queue is a deferred-type continuation of another */
      bool is_continuation_deferred{false};

      /** Function to be called for deferred evaulation of a single value if this queue is a continuation */
      std::function<void(void)> continuation_process_deferred;

      /** Function to be called for deferred evaulation of a single value if this queue is a continuation. This version
       *  is supposed to wait until there is data to process. */
      std::function<void(void)> continuation_process_deferred_wait;

      /** Flag whether this future_queue is a async-type continuation of another */
      bool is_continuation_async{false};

      /** Thread handling async-type continuations */
      std::thread continuation_process_async;

      /** Notification queue used to realise a wait_any logic. This queue is shared between all queues participating in
       *  the same when_any. */
      future_queue<size_t> notifyerQueue;

      /** counter for the number of elements in the queue before when_any has added the notifyerQueue */
      std::atomic<size_t> notifyerQueue_previousData;

    };

    /** Internal class for holding the data which is shared between multiple instances of the same queue. This class is
     *  depeding on the data type and is used by the future_queue class. */
    template<typename T>
    struct shared_state : shared_state_base {
      shared_state(size_t length)
      : shared_state_base(length), buffers(length+1)
      {}

      ~shared_state() {
        if(is_continuation_async) {               // FIXME this is never called, as the async thread still holds the reference!
          continuation_process_async.join();
        }
      }

      /** vector of buffers - allocation is done in the constructor */
      std::vector<T> buffers;

    };

    /** Specialisation of the shared_state class for the type void. */
    template<>
    struct shared_state<void> : shared_state_base {
      shared_state(size_t length)
      : shared_state_base(length)
      {}

      ~shared_state() {
        if(is_continuation_async) {               // FIXME this is never called, as the async thread still holds the reference!
          continuation_process_async.join();
        }
      }

    };

  } // namespace detail

  /*********************************************************************************************************************/

  /** This function expects two forward iterators pointing to a region of a container of future_queue objects. It
   *  returns a future_queue which will receive the index of each queue relative to the iterator begin when the
   *  respective queue has new data available for reading. This way the returned queue can be used to get notified about
   *  each data written to any of the queues. The order of the indices in this queue is guaranteed to be in the same
   *  order the data has been written to the queues. If the same queue gets written to multiple times its index will be
   *  present in the returned queue the same number of times.
   *
   *  Behaviour is unspecified if, after the call to when_any(), data is popped from one of the participating queues
   *  without retreiving its index previously from the returned queue. Behaviour is also unspecified if the same queue
   *  is passed to different calls to this function, or occurres multiple times.
   *
   *  If push_overwrite() is used on one of the participating queues, the notifications received through the returned
   *  queue might be in a different order (i.e. when data is overwritten, the corresponding queue index is not moved to
   *  the correct place later in the notfication queue). Also, a notification for a value written to a queue with
   *  push_overwrite() might appear in the notification queue before the value can be retrieved from the data queue. It
   *  is therefore recommended to use pop_wait() to retrieve the values from the data queues if push_overwrite() is
   *  used. Otherwise failed pop() have to be retried until the data is received.
   *
   *  If data is already available in the queues before calling when_any(), the appropriate number of notifications are
   *  placed in the notifyer queue in arbitrary order. */
  template<typename ITERATOR_TYPE>
  future_queue<size_t> when_any(ITERATOR_TYPE begin, ITERATOR_TYPE end) {

      // Add lengthes of all queues - this will be the length of the notification queue
      size_t summedLength = 0;
      for(ITERATOR_TYPE it = begin; it != end; ++it) summedLength += it->size();

      // Create a notification queue in a shared pointer, so we can hand it on to the queues
      future_queue<size_t> notifyerQueue(summedLength);

      // Distribute the pointer to the notification queue to all participating queues
      size_t index = 0;
      for(ITERATOR_TYPE it = begin; it != end; ++it) {
        atomic_store(&(it->d->notifyerQueue), notifyerQueue);
        // at this point, queue.notifyerQueue_previousData will no longer be modified by the sender side
        size_t nPreviousValues = it->d->notifyerQueue_previousData;
        it->d->when_any_index = index;
        for(size_t i=0; i<nPreviousValues; ++i) notifyerQueue.push(index);
        ++index;
      }

      return notifyerQueue;
  }

  /*********************************************************************************************************************/

  /** This function expects two forward iterators pointing to a region of a container of future_queue objects. It
   *  returns a future_queue<void> which will receive a notification when all of the queues in the region have received
   *  a new value. */
  template<typename ITERATOR_TYPE>
  future_queue<void> when_all(ITERATOR_TYPE begin, ITERATOR_TYPE end) {

      // Create a notification queue in a shared pointer, so we can hand it on to the queues
      future_queue<void> notifyerQueue(1);

      // copy the list of participating queues
      std::vector<future_queue_base> participants;
      for(auto it=begin; it!=end; ++it) participants.push_back(*it);

      notifyerQueue.d->continuation_process_deferred = std::function<void(void)>( [notifyerQueue, participants] () mutable {
        bool empty = false;
        for(auto &q: participants) {
          if(q.empty()) {
            empty = true; break;
          }
        }
        if(!empty) notifyerQueue.push();
      } );

      notifyerQueue.d->continuation_process_deferred_wait = std::function<void(void)>( [notifyerQueue, participants] () mutable {
        for(auto &q: participants) q.wait();
        notifyerQueue.push();
      } );

      notifyerQueue.d->is_continuation_deferred = true;

      return notifyerQueue;
  }

  /*********************************************************************************************************************/

  namespace detail {

    /** Helper function to realise the data assignment depending on the selected FEATURES tags. The last dummy argument
     *  is just used to realise overloads for the different tags (as C++ does not know partial template specialisations
     *  for functions). */
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

  inline size_t future_queue_base::write_available() const {
    size_t l_writeIndex = d->writeIndex;
    size_t l_readIndex = d->readIndex;
    if(l_writeIndex - l_readIndex < d->nBuffers-1) {
      return d->nBuffers - (l_writeIndex - l_readIndex) - 1;
    }
    else {
      return 0;
    }
  }

  inline size_t future_queue_base::read_available() const {
    return d->readIndexMax - d->readIndex;
  }

  inline bool future_queue_base::push_exception(std::exception_ptr exception) {
    size_t myIndex;
    if(!obtain_write_slot(myIndex)) return false;
    d->exceptions[myIndex % d->nBuffers] = exception;
    assert(!d->semaphores[myIndex % d->nBuffers].is_ready());
    d->semaphores[myIndex % d->nBuffers].unlock();
    update_read_index_max();
    // send notification if requested
    auto notify = atomic_load(&d->notifyerQueue);
    if(notify.d) {
      bool nret = notify.push(d->when_any_index);
      (void)nret;
      assert(nret == true);
    }
    else {
      d->notifyerQueue_previousData++;
    }
    return true;
  }

  inline bool future_queue_base::empty() {
    if(d->hasFrontOwnership) return false;
    if(d->is_continuation_deferred) d->continuation_process_deferred();
    if(d->semaphores[d->readIndex%d->nBuffers].is_ready_and_reset()) {
      d->hasFrontOwnership = true;
      return false;
    }
    return true;
  }

  inline void future_queue_base::wait() {
    if(d->hasFrontOwnership) return;
    if(d->is_continuation_deferred) d->continuation_process_deferred_wait();
    d->semaphores[d->readIndex%d->nBuffers].wait_and_reset();
    d->hasFrontOwnership = true;
  }

  inline size_t future_queue_base::size() const {
    return d->nBuffers - 1;
  }

  inline future_queue_base::future_queue_base(const std::shared_ptr<detail::shared_state_base> &d_ptr_)
  : d(d_ptr_) {}

  inline future_queue_base::future_queue_base() : d(nullptr) {}

  inline bool future_queue_base::obtain_write_slot(size_t &index) {
    index = d->writeIndex;
    while(true) {
      if(index >= d->readIndex+d->nBuffers - 1) return false;   // queue is full
      bool success = d->writeIndex.compare_exchange_weak(index, index+1);
      if(success) break;
    }
    return true;
  }

  inline void future_queue_base::update_read_index_max() {
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

  /*********************************************************************************************************************/

  template<typename T, typename FEATURES>
  future_queue<T,FEATURES>::future_queue(size_t length)
  : future_queue_base(std::make_shared<detail::shared_state<T>>(length))
  {}

  template<typename T, typename FEATURES>
  future_queue<T,FEATURES>::future_queue() {}

  template<typename T, typename FEATURES>
  template<typename U, typename std::enable_if< std::is_same<T,U>::value && !std::is_same<U, void>::value, int >::type>
  bool future_queue<T,FEATURES>::push(U&& t) {
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
    auto notify = atomic_load(&d->notifyerQueue);
    if(notify.d) {
      bool nret = notify.push(d->when_any_index);
      (void)nret;
      assert(nret == true);
    }
    else {
      d->notifyerQueue_previousData++;
    }
    return true;
  }

  template<typename T, typename FEATURES>
  template<typename U, typename std::enable_if< !std::is_same<U, void>::value, int >::type>
  bool future_queue<T,FEATURES>::push(const U& t) {
    return push(T(t));
  }

  template<typename T, typename FEATURES>
  bool future_queue<T,FEATURES>::push(void) {
    static_assert( std::is_same<T, void>::value, "future_queue<T,FEATURES>::push(void) may only be called for T = void." );
    size_t myIndex;
    if(!obtain_write_slot(myIndex)) return false;
    assert(!d->semaphores[myIndex % d->nBuffers].is_ready());
    d->semaphores[myIndex % d->nBuffers].unlock();
    update_read_index_max();
    // send notification if requested
    auto notify = atomic_load(&d->notifyerQueue);
    if(notify.d) {
      bool nret = notify.push(d->when_any_index);
      (void)nret;
      assert(nret == true);
    }
    else {
      d->notifyerQueue_previousData++;
    }
    return true;
  }

  template<typename T, typename FEATURES>
  template<typename U, typename std::enable_if< std::is_same<T,U>::value && !std::is_same<U, void>::value, int >::type>
  bool future_queue<T,FEATURES>::push_overwrite(U&& t) {
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
      auto notify = atomic_load(&d->notifyerQueue);
      if(notify.d) {
        bool nret = notify.push(d->when_any_index);
        (void)nret;
        assert(nret == true);
      }
      else {
        d->notifyerQueue_previousData++;
      }
    }
    return ret;
  }

  template<typename T, typename FEATURES>
  template<typename U, typename std::enable_if< !std::is_same<U, void>::value, int >::type>
  bool future_queue<T,FEATURES>::push_overwrite(const U& t) {
    return push_overwrite(T(t));
  }

  template<typename T, typename FEATURES>
  template<typename U, typename std::enable_if< std::is_same<T,U>::value && !std::is_same<U, void>::value, int >::type>
  bool future_queue<T,FEATURES>::pop(U& t) {
    if(d->is_continuation_deferred && !d->hasFrontOwnership) d->continuation_process_deferred();
    if( d->hasFrontOwnership || d->semaphores[d->readIndex%d->nBuffers].is_ready_and_reset() ) {
      std::exception_ptr e;
      if(d->exceptions[d->readIndex%d->nBuffers]) {
        e = d->exceptions[d->readIndex%d->nBuffers];
      }
      else {
        detail::data_assign(
          t,
          std::move(static_cast<detail::shared_state<T>*>(future_queue_base::d.get())->buffers[d->readIndex%d->nBuffers]),
          FEATURES()
        );
      }
      assert(d->readIndex < d->writeIndex);
      d->readIndex++;
      d->hasFrontOwnership = false;
      d->notifyerQueue_previousData--;
      if(e) std::rethrow_exception(e);
      return true;
    }
    else {
      return false;
    }
  }

  template<typename T, typename FEATURES>
  bool future_queue<T,FEATURES>::pop() {
    if(d->is_continuation_deferred && !d->hasFrontOwnership) d->continuation_process_deferred();
    if( d->hasFrontOwnership || d->semaphores[d->readIndex%d->nBuffers].is_ready_and_reset() ) {
      std::exception_ptr e;
      if(d->exceptions[d->readIndex%d->nBuffers]) {
        e = d->exceptions[d->readIndex%d->nBuffers];
      }
      assert(d->readIndex < d->writeIndex);
      d->readIndex++;
      d->hasFrontOwnership = false;
      d->notifyerQueue_previousData--;
      if(e) std::rethrow_exception(e);
      return true;
    }
    else {
      return false;
    }
  }

  template<typename T, typename FEATURES>
  template<typename U, typename std::enable_if< std::is_same<T,U>::value && !std::is_same<U, void>::value, int >::type>
  void future_queue<T,FEATURES>::pop_wait(U& t) {
    if(!d->hasFrontOwnership) {
      if(d->is_continuation_deferred) d->continuation_process_deferred_wait();
      d->semaphores[d->readIndex%d->nBuffers].wait_and_reset();
    }
    else {
      d->hasFrontOwnership = false;
    }
    std::exception_ptr e;
    if(d->exceptions[d->readIndex%d->nBuffers]) {
      e = d->exceptions[d->readIndex%d->nBuffers];
    }
    else {
      detail::data_assign(
        t,
        std::move(static_cast<detail::shared_state<T>*>(future_queue_base::d.get())->buffers[d->readIndex%d->nBuffers]),
        FEATURES()
      );
    }
    assert(d->readIndex < d->writeIndex);
    d->readIndex++;
    d->notifyerQueue_previousData--;
    if(e) std::rethrow_exception(e);
  }

  template<typename T, typename FEATURES>
  void future_queue<T,FEATURES>::pop_wait() {
    if(!d->hasFrontOwnership) {
      if(d->is_continuation_deferred) d->continuation_process_deferred_wait();
      d->semaphores[d->readIndex%d->nBuffers].wait_and_reset();
    }
    else {
      d->hasFrontOwnership = false;
    }
    std::exception_ptr e;
    if(d->exceptions[d->readIndex%d->nBuffers]) {
      e = d->exceptions[d->readIndex%d->nBuffers];
    }
    assert(d->readIndex < d->writeIndex);
    d->readIndex++;
    d->notifyerQueue_previousData--;
    if(e) std::rethrow_exception(e);
  }

  template<typename T, typename FEATURES>
  template<typename U, typename std::enable_if< std::is_same<T,U>::value && !std::is_same<U, void>::value, int >::type>
  const U& future_queue<T,FEATURES>::front() const {
    assert(d->hasFrontOwnership);
    if(d->exceptions[d->readIndex%d->nBuffers]) std::rethrow_exception(d->exceptions[d->readIndex%d->nBuffers]);
    return static_cast<detail::shared_state<T>*>(future_queue_base::d.get())->buffers[d->readIndex%d->nBuffers];
  }

  namespace detail {
    // helper functions used inside future_queue::then() - needed instead of a lambda since T might be void
    template<typename T, typename FEATURES, typename TOUT, typename CALLABLE>
    struct continuation_process_deferred {
      continuation_process_deferred(future_queue<T,FEATURES> q_input_, future_queue<TOUT> q_output_, CALLABLE callable_)
      : q_input(q_input_), q_output(q_output_), callable(callable_) {}
      void operator()() {
        T inp;
        bool got_data = q_input.pop(inp);
        if(got_data) q_output.push(callable(inp));
      }
      future_queue<T,FEATURES> q_input;
      future_queue<TOUT> q_output;
      CALLABLE callable;
    };
    template<typename FEATURES, typename TOUT, typename CALLABLE>
    struct continuation_process_deferred<void, FEATURES, TOUT, CALLABLE> {
      continuation_process_deferred(future_queue<void,FEATURES> q_input_, future_queue<TOUT> q_output_, CALLABLE callable_)
      : q_input(q_input_), q_output(q_output_), callable(callable_) {}
      void operator()() {
        bool got_data = q_input.pop();
        if(got_data) q_output.push(callable());
      }
      future_queue<void,FEATURES> q_input;
      future_queue<TOUT> q_output;
      CALLABLE callable;
    };
    template<typename FEATURES, typename CALLABLE>
    struct continuation_process_deferred<void, FEATURES, void, CALLABLE> {
      continuation_process_deferred(future_queue<void,FEATURES> q_input_, future_queue<void> q_output_, CALLABLE callable_)
      : q_input(q_input_), q_output(q_output_), callable(callable_) {}
      void operator()() {
        bool got_data = q_input.pop();
        callable();
        if(got_data) q_output.push();
      }
      future_queue<void,FEATURES> q_input;
      future_queue<void> q_output;
      CALLABLE callable;
    };
    template<typename T, typename FEATURES, typename TOUT, typename CALLABLE>
    continuation_process_deferred<T,FEATURES,TOUT,CALLABLE> make_continuation_process_deferred(
        future_queue<T,FEATURES> q_input, future_queue<TOUT> q_output, CALLABLE callable) {
      return {q_input,q_output,callable};
    }

    template<typename T, typename FEATURES, typename TOUT, typename CALLABLE>
    struct continuation_process_deferred_wait {
      continuation_process_deferred_wait(future_queue<T,FEATURES> q_input_, future_queue<TOUT> q_output_, CALLABLE callable_)
      : q_input(q_input_), q_output(q_output_), callable(callable_) {}
      void operator()() {
        T inp;
        q_input.pop_wait(inp);
        q_output.push(callable(inp));
      }
      future_queue<T,FEATURES> q_input;
      future_queue<TOUT> q_output;
      CALLABLE callable;
    };
    template<typename FEATURES, typename TOUT, typename CALLABLE>
    struct continuation_process_deferred_wait<void, FEATURES, TOUT, CALLABLE> {
      continuation_process_deferred_wait(future_queue<void,FEATURES> q_input_, future_queue<TOUT> q_output_, CALLABLE callable_)
      : q_input(q_input_), q_output(q_output_), callable(callable_) {}
      void operator()() {
        q_input.pop_wait();
        q_output.push(callable());
      }
      future_queue<void,FEATURES> q_input;
      future_queue<TOUT> q_output;
      CALLABLE callable;
    };
    template<typename FEATURES, typename CALLABLE>
    struct continuation_process_deferred_wait<void, FEATURES, void, CALLABLE> {
      continuation_process_deferred_wait(future_queue<void,FEATURES> q_input_, future_queue<void> q_output_, CALLABLE callable_)
      : q_input(q_input_), q_output(q_output_), callable(callable_) {}
      void operator()() {
        q_input.pop_wait();
        callable();
        q_output.push();
      }
      future_queue<void,FEATURES> q_input;
      future_queue<void> q_output;
      CALLABLE callable;
    };
    template<typename T, typename FEATURES, typename TOUT, typename CALLABLE>
    continuation_process_deferred_wait<T,FEATURES,TOUT,CALLABLE> make_continuation_process_deferred_wait(
        future_queue<T,FEATURES> q_input, future_queue<TOUT> q_output, CALLABLE callable) {
      return {q_input,q_output,callable};
    }

    template<typename T, typename FEATURES, typename TOUT, typename CALLABLE>
    struct continuation_process_async {
      continuation_process_async(future_queue<T,FEATURES> q_input_, future_queue<TOUT> q_output_, CALLABLE callable_)
      : q_input(q_input_), q_output(q_output_), callable(callable_) {}
      void operator()() {
        while(true) {         // FIXME how to shutdown properly
          T inp;
          q_input.pop_wait(inp);
          q_output.push(callable(inp));
          // TODO how to handle full output queues?
        }
      }
      future_queue<T,FEATURES> q_input;
      future_queue<TOUT> q_output;
      CALLABLE callable;
    };
    template<typename FEATURES, typename TOUT, typename CALLABLE>
    struct continuation_process_async<void, FEATURES, TOUT, CALLABLE> {
      continuation_process_async(future_queue<void,FEATURES> q_input_, future_queue<TOUT> q_output_, CALLABLE callable_)
      : q_input(q_input_), q_output(q_output_), callable(callable_) {}
      void operator()() {
        while(true) {        // FIXME how to shutdown properly
          q_input.pop_wait();
          q_output.push(callable());
          // TODO how to handle full output queues?
        }
      }
      future_queue<void,FEATURES> q_input;
      future_queue<TOUT> q_output;
      CALLABLE callable;
    };
    template<typename FEATURES, typename CALLABLE>
    struct continuation_process_async<void, FEATURES, void, CALLABLE> {
      continuation_process_async(future_queue<void,FEATURES> q_input_, future_queue<void> q_output_, CALLABLE callable_)
      : q_input(q_input_), q_output(q_output_), callable(callable_) {}
      void operator()() {
        while(true) {        // FIXME how to shutdown properly
          q_input.pop_wait();
          callable();
          q_output.push();
          // TODO how to handle full output queues?
        }
      }
      future_queue<void,FEATURES> q_input;
      future_queue<void> q_output;
      CALLABLE callable;
    };
    template<typename T, typename FEATURES, typename TOUT, typename CALLABLE>
    continuation_process_async<T,FEATURES,TOUT,CALLABLE> make_continuation_process_async(
        future_queue<T,FEATURES> q_input, future_queue<TOUT> q_output, CALLABLE callable) {
      return {q_input,q_output,callable};
    }
  } // namespace detail

  template<typename T, typename FEATURES>
  template<typename T2, typename FEATURES2, typename CALLABLE>
  future_queue<T2,FEATURES2> future_queue<T,FEATURES>::then(CALLABLE callable, std::launch policy) {
      future_queue<T,FEATURES> q_input(*this);
      if(policy == std::launch::deferred) {
        future_queue<T2,FEATURES2> q_output(1);

        q_output.d->continuation_process_deferred = detail::make_continuation_process_deferred(q_input, q_output, callable);
        q_output.d->continuation_process_deferred_wait = detail::make_continuation_process_deferred_wait(q_input, q_output, callable);

        q_output.d->is_continuation_deferred = true;
        return q_output;
      }
      else {
        future_queue<T2,FEATURES2> q_output(size());
        q_output.d->continuation_process_async = std::thread(detail::make_continuation_process_async(q_input, q_output, callable));
        q_output.d->is_continuation_async = true;
        return q_output;
      }
  }

} // namespace cppext

#endif // FUTURE_QUEUE_HPP
