#ifndef FUTURE_QUEUE_HPP
#define FUTURE_QUEUE_HPP

#include <atomic>
#include <cassert>
#include <future> // just for std::launch
#include <vector>

#include "semaphore.hpp"

namespace cppext {

  /*********************************************************************************************************************/

  /** Feature tag for future_queue: use std::move to store and retreive data
   * to/from the queue */
  class MOVE_DATA {};

  /** Feature tag for future_queue: use std::swap to store and retreive data
   * to/from the queue */
  class SWAP_DATA {};

  namespace detail {
    /** Exception to be pushed into the queue to signal a termination request for
     * the internal thread of an async continuation. This is done automatically
     * during destruction of the corresponding queue. */
    class TerminateInternalThread {};
  } // namespace detail

  /*********************************************************************************************************************/

  namespace detail {
    struct shared_state_base;

    template<typename T>
    struct shared_state;

    template<typename T, typename FEATURES, typename TOUT, typename CALLABLE>
    struct continuation_process_async;

    /** shared_ptr-like smart pointer type for referencing the shared_state. This is
     * required (instead or in addition to using a shared_ptr), since in case of a
     * async continuation the internal thread always holds a reference to the
     * shared_state but should be destroyed when the last reference (outside the
     * internal thread) gets destroyed. */
    struct shared_state_ptr {
      /// Default constructor: create empty pointer
      shared_state_ptr();

      /// Copy constructor
      shared_state_ptr(const shared_state_ptr& other);

      /// Copy by assignment
      shared_state_ptr& operator=(const shared_state_ptr& other);

      /// Destructor
      ~shared_state_ptr();

      /// Create new shared_state for type T
      template<typename T>
      void make_new(size_t length);

      /// Dereferencing operator
      shared_state_base* operator->();
      const shared_state_base* operator->() const;

      /// Cast into shared state for type T
      template<typename T>
      shared_state<T>* cast();

      /// Check if pointer is initialised
      operator bool() const;

      /// Check if two pointers are identical
      bool operator==(const shared_state_ptr& other) const;

      /// Obtain the target pointer
      shared_state_base* get() const;

      /// Set the target pointer without incrementing the reference counter
      void set(shared_state_base* ptr_);

     private:
      /// Decrease reference counter and check if target shared_state should be deleted
      void free();

      /// Target pointer
      shared_state_base* ptr;
    };

    template<typename T>
    shared_state_ptr make_shared_state(size_t length) {
      shared_state_ptr p;
      p.make_new<T>(length);
      return p;
    }

  } // namespace detail

  template<typename T, typename FEATURES>
  class future_queue;

  /*********************************************************************************************************************/

  /** Type-independent base class for future_queue which does not depend on the
   * template argument. For a description see future_queue. */
  class future_queue_base {
   public:
    /** Number of push operations which can be performed before the queue is full.
     * Note that the result may be inacurate e.g. in multi-producer contexts. */
    size_t write_available() const;

    /** Number of pop operations which can be performed before the queue is empty.
     * Note that the result can be inaccurate in case the sender uses
     * push_overwrite(). If a guarantee is required that a readable element is
     * present before accessing it through pop() or front(), use empty(). */
    size_t read_available() const;

    /** Push an exception pointer (inplace of a value) into the queue. The
     * exception gets thrown by pop()/pop_wait()/front() when the receiver reads
     * the corresponding queue element. */
    bool push_exception(std::exception_ptr exception);

    /** Like push_exception() but overwrite the last pushed value in case the
     * queue is full. See also push_overwrite() for more information. */
    bool push_overwrite_exception(std::exception_ptr exception);

    /** Check if there is currently no data on the queue. If the queue contains
     * data (i.e. true will be returned), the function will guarantee that this
     * data can be accessed later e.g. thorugh front() or pop(). This guarantee
     * holds even if the sender uses pop_overwrite(). */
    bool empty();

    /** Wait until the queue is not empty. This function guarantees similar like
     * empty() that after this call data can be accessed e.g. through front() or
     * pop(). */
    void wait();

    /** return length of the queue */
    size_t size() const;

    /** Check whether two future_queue instances use the same shared state, i.e.
     * represent the same queue. */
    bool operator==(const future_queue_base& other) const;
    bool operator!=(const future_queue_base& other) const;

   protected:
    future_queue_base(const detail::shared_state_ptr& d_ptr_);

    future_queue_base();

    /** reserve next available write slot. Returns false if no free slot is
     * available or true on success. */
    bool obtain_write_slot(size_t& index);

    /** update readIndexMax after a write operation was completed */
    void update_read_index_max();

    /** Set the notification queue in the shared state, as done in when_any. Returns the number of data in the queue
     *  at the time the notification queue was set (atomically). */
    size_t setNotificationQueue(future_queue<size_t, MOVE_DATA>& notificationQueue, size_t indexToSend);

    /** Atomically return the notification queue or increment the "previous data" counter (for wait_any).
     *  The counter is incremented if no notification queue is set. This allows wait_any to determine how many pending
     *  values are in the value queue, so it can place corresponding notifications to the notification queue.
     *
     *  The call to this function must take place before signalling the receiving side of the queue, so the
     *  corresponding call to decrement_previous_data_counter() is properly synchronised. */
    cppext::detail::shared_state_base* get_notification_queue();

    /** Send notification to notification queue (if not nullptr). The notification queue must be obtained before
     *  through get_notification_queue().
     *
     *  The call to this function must take place after signalling the receiving side of the queue, so the value is
     *  available right away when receiving the notification. */
    void send_notification(cppext::detail::shared_state_base* notification_queue);

    /** Decrement the "previous data" counter used in when_any(). */
    void decrement_previous_data_counter();

    /** pointer to data used to allow sharing the queue (create multiple copies
     * which all refer to the same queue). */
    detail::shared_state_ptr d;

    template<typename T, typename FEATURES>
    friend class ::cppext::future_queue;

    template<typename ITERATOR_TYPE>
    friend future_queue<size_t, MOVE_DATA> when_any(ITERATOR_TYPE begin, ITERATOR_TYPE end);

    template<typename ITERATOR_TYPE>
    friend future_queue<void, MOVE_DATA> when_all(ITERATOR_TYPE begin, ITERATOR_TYPE end);

    friend struct detail::shared_state_base;
    friend struct detail::shared_state_ptr;

    template<typename T, typename FEATURES, typename TOUT, typename CALLABLE>
    friend struct detail::continuation_process_async;
  };

  /*********************************************************************************************************************/

  /**
   *  A lockfree multi-producer single-consumer queue of a fixed length which the
   * receiver can wait on in case the queue is empty. This is similiar like using
   * a lockfree queue of futures but has better performance. In addition the queue
   *  allows the sender to overwrite the last written element in case the queue is
   * full. The receiver can also use the function when_any() to get notified when
   * any of the given future_queues is not empty.
   *
   *  The template parameter T specifies the type of the user data stored in the
   * queue. The optional second template parameter takes one of the feature tags.
   * Currently two options are supported:
   *
   *   - MOVE_DATA (default): Type T must have a move constructor. To place
   * objects on the queue and to retrieve them from the queue, a move operation is
   * performed.
   *   - SWAP_DATA:           The function std::swap() must be overloaded for the
   * type T. When placing objects on the queue, std::swap() is called to exchange
   * the new object with the object currently on the internal queue buffer. This
   * allows avoiding unnecessary memory allocations e.g. when storing std::vector
   * on the queue, especially if all vectors have the same size.
   *
   *  In both cases, T must be default constructible. Upon creation of the queue
   * all internal buffers will be filled with default constructed elements.
   */
  template<typename T, typename FEATURES = MOVE_DATA>
  class future_queue : public future_queue_base {
   public:
    /** The length specifies how many objects the queue can contain at a time.
     * Internally additional buffers will be
     *  allocated-> All buffers are allocated upon construction, so no dynamic
     * memory allocation is required later. */
    future_queue(size_t length);

    /** The default constructor creates only a place holder which can later be
     * assigned with a properly constructed queue. */
    future_queue();

    /** Copy constructor: After copying the object both *this and the other object
     * will refer to the same queue. */
    future_queue(const future_queue& other) = default;

    /** Copy assignment operator: After the assignment both *this and the other
     * object will refer to the same queue. */
    future_queue& operator=(const future_queue& other) = default;

    /** Push object t to the queue. Returns true if successful and false if queue
     * is full. */
    template<typename U = T,
        typename std::enable_if<std::is_same<T, U>::value && !std::is_same<U, void>::value, int>::type = 0>
    bool push(U&& t);
    template<typename U = T,
        typename std::enable_if<!std::is_same<U, void>::value && std::is_copy_constructible<T>::value, int>::type = 0>
    bool push(const U& t);

    /** This version of push() is valid only for T=void */
    bool push(void);

    /** Push object t to the queue. If the queue is full, the last element will be
     * overwritten and false will be returned. If no data had to be overwritten,
     * true is returned.
     *
     *  When using this function, the queue must have a length of at least 2.
     *
     *  Note: when used in a multi-producer context and false is returned, it is
     * not defined whether other data or data written in this call to
     * push_overwrite() has been discarded. */
    template<typename U = T,
        typename std::enable_if<std::is_same<T, U>::value && !std::is_same<U, void>::value, int>::type = 0>
    bool push_overwrite(U&& t);

    template<typename U = T,
        typename std::enable_if<!std::is_same<U, void>::value && std::is_copy_constructible<T>::value, int>::type = 0>
    bool push_overwrite(const U& t);

    /** This version of push_overwrite() is valid only for T=void */
    bool push_overwrite();

    /** Pop object off the queue and store it in t. If no data is available, false
     * is returned */
    template<typename U = T,
        typename std::enable_if<std::is_same<T, U>::value && !std::is_same<U, void>::value, int>::type = 0>
    bool pop(U& t);

    bool pop();

    /** Pop object off the queue and store it in t. This function will block until
     * data is available. */
    template<typename U = T,
        typename std::enable_if<std::is_same<T, U>::value && !std::is_same<U, void>::value, int>::type = 0>
    void pop_wait(U& t);

    void pop_wait();

    /** Obtain the front element of the queue without removing it. It is mandatory to make sure that data is available
     *  in the queue by calling empty() before calling this function. 
     *
     *  Note: No const variant exists, since empty() already changes the state of the queue internally (to acquire
     *  ownership on the front element, so push_overwrite() can no longer overwrite it). Without a const variant of
        empty(), a const variant of front() would be unusable. */
    template<typename U = T,
        typename std::enable_if<std::is_same<T, U>::value && !std::is_same<U, void>::value, int>::type = 0>
    U& front();

    /** Add continuation: Whenever there is a new element in the queue, process it
     * with the callable and put the result into a new queue. The new queue will
     * be returned by this function.
     *
     *  The signature of the callable must be "T2(T)", i.e. it has a single
     * argument of the value type T of the queue then() is called on, and the
     * return type matches the value type of the returned queue.
     *
     *  Two different launch policies can be selected:
     *   - std::launch::async will launch a new thread and trigger data processing
     * asynchronously in the background. Each value will be processed in the order
     * they are pushed to the queue and in the same thread.
     *   - std::launch::deferred will defer data processing until the data is
     * accessed on the resulting queue. Checking the presence through empty() is
     * already counted an access. If the same data is accessed multiple times
     * (e.g. by calling front() several times), the callable is only executed
     * once. If neither std::launch::async (which is the default) nor
     * std::launch::deferred is specified, the behaviour is undefined. */
    template<typename T2, typename FEATURES2 = MOVE_DATA, typename CALLABLE>
    future_queue<T2, FEATURES2> then(CALLABLE callable, std::launch policy = std::launch::async);

    typedef T value_type;
  };

  /*********************************************************************************************************************/

  namespace detail {

    struct when_any_notification_info {
      /** Notification queue used to realise a wait_any logic. This queue is shared between all queues participating
       *  in the same when_any. */
      cppext::detail::shared_state_base* notifyerQueue{nullptr};

      /** counter for the number of elements in the queue before when_any has added the notifyerQueue */
      size_t notifyerQueue_previousData{0};
    };

    /** Internal base class for holding the data which is shared between multiple
     * instances of the same queue. The base
     *  class does not depend on the data type and is used by future_queue_base. */
    struct shared_state_base {
      shared_state_base(size_t length)
      : nBuffers(length + 1), semaphores(length + 1), exceptions(length + 1), writeIndex(0), readIndexMax(0),
        readIndex(0), hasFrontOwnership(false) {}

      /** Destructor must be virtual so the destructor of the derived class gets
       * called. */
      virtual ~shared_state_base() {}

      /** reference count. See shared_state_ptr for further documentation. */
      std::atomic<size_t> reference_count{0};

      /** index used in wait_any to identify the queue */
      size_t when_any_index;

      /** the number of buffers we have allocated */
      size_t nBuffers;

      /** vector of semaphores corresponding to the buffers which allows the
       * receiver to wait for new data */
      std::vector<semaphore> semaphores;

      /** vector of exception pointers, can be set instead of values through
       * push_exception() */
      std::vector<std::exception_ptr> exceptions;

      /** index of the element which will be next written
       *  @todo FIXME protect handling of all indices against overruns of size_t! */
      std::atomic<size_t> writeIndex;

      /** maximum index which the receiver is currently allowed to read (after
       * checking it semaphore). Often equal to
       *  writeIndex, unless write operations are currently in progress */
      std::atomic<size_t> readIndexMax;

      /** index of the element which will be next read */
      std::atomic<size_t> readIndex;

      /** Flag if the receiver has already ownership over the front element. This
       * flag may only be used by the receiver. */
      bool hasFrontOwnership;

      /** Flag whether this future_queue is a deferred-type continuation of another
       */
      bool is_continuation_deferred{false};

      /** Function to be called for deferred evaulation of a single value if this
       * queue is a continuation */
      std::function<void(void)> continuation_process_deferred;

      /** Function to be called for deferred evaulation of a single value if this
       * queue is a continuation. This version is supposed to wait until there is
       * data to process. */
      std::function<void(void)> continuation_process_deferred_wait;

      /** Flag whether this future_queue is a async-type continuation of another */
      bool is_continuation_async{false};

      /** Thread handling async-type continuations */
      std::thread continuation_process_async;

      /** Flag whether the internal thread continuation_process_async has been
       * terminated */
      std::atomic<bool> continuation_process_async_terminated{false};

      /** Flag whether this future_queue is a when_all-type continuation (of many
       * other) */
      bool is_continuation_when_all{false};

      /** If either is_continuation_deferred or is_continuation_async is true, this
       * will point to the original queue of which *this is the continuation. */
      future_queue_base continuation_origin;

      std::atomic<when_any_notification_info> when_any_notification;
    };

    /** Internal class for holding the data which is shared between multiple
     * instances of the same queue. This class is depeding on the data type and is
     * used by the future_queue class. */
    template<typename T>
    struct shared_state : shared_state_base {
      shared_state(size_t length) : shared_state_base(length), buffers(length + 1) {}

      /** vector of buffers - allocation is done in the constructor */
      std::vector<T> buffers;
    };

    /** Specialisation of the shared_state class for the type void. */
    template<>
    struct shared_state<void> : shared_state_base {
      shared_state(size_t length) : shared_state_base(length) {}
    };

  } // namespace detail

  /*********************************************************************************************************************/
  /*********************************************************************************************************************/
  /** Implementations of non-member functions */
  /*********************************************************************************************************************/
  /*********************************************************************************************************************/

  /** This function expects two forward iterators pointing to a region of a
   * container of future_queue objects. It returns a future_queue which will
   * receive the index of each queue relative to the iterator begin when the
   *  respective queue has new data available for reading. This way the returned
   * queue can be used to get notified about each data written to any of the
   * queues. The order of the indices in this queue is guaranteed to be in the
   * same order the data has been written to the queues. If the same queue gets
   * written to multiple times its index will be present in the returned queue the
   * same number of times.
   *
   *  Behaviour is unspecified if, after the call to when_any(), data is popped
   * from one of the participating queues without retreiving its index previously
   * from the returned queue. Behaviour is also unspecified if the same queue is
   * passed to different calls to this function, or occurres multiple times.
   *
   *  If push_overwrite() is used on one of the participating queues, the
   * notifications received through the returned queue might be in a different
   * order (i.e. when data is overwritten, the corresponding queue index is not
   * moved to the correct place later in the notfication queue). Also, a
   * notification for a value written to a queue with push_overwrite() might
   * appear in the notification queue before the value can be retrieved from the
   * data queue. It is therefore recommended to use pop_wait() to retrieve the
   * values from the data queues if push_overwrite() is used. Otherwise failed
   * pop() have to be retried until the data is received.
   *
   *  If data is already available in the queues before calling when_any(), the
   * appropriate number of notifications are placed in the notifyer queue in
   * arbitrary order. */
  template<typename ITERATOR_TYPE>
  future_queue<size_t> when_any(ITERATOR_TYPE begin, ITERATOR_TYPE end) {
    // Add lengthes of all queues - this will be the length of the notification
    // queue
    size_t summedLength = 0;
    for(ITERATOR_TYPE it = begin; it != end; ++it) summedLength += it->size();

    // Create a notification queue, so we can hand it on to the queues
    future_queue<size_t> notifyerQueue(summedLength);

    // Distribute the pointer to the notification queue to all participating
    // queues
    size_t index = 0;
    for(ITERATOR_TYPE it = begin; it != end; ++it) {
      size_t nPreviousValues = it->setNotificationQueue(notifyerQueue, index);
      for(size_t i = 0; i < nPreviousValues; ++i) notifyerQueue.push(index);
      ++index;
    }

    return notifyerQueue;
  }

  /*********************************************************************************************************************/

  /** This function expects two forward iterators pointing to a region of a
   * container of future_queue objects. It returns a future_queue<void> which will
   * receive a notification when all of the queues in the region have received a
   * new value. */
  template<typename ITERATOR_TYPE>
  future_queue<void> when_all(ITERATOR_TYPE begin, ITERATOR_TYPE end) {
    // Create a notification queue in a shared pointer, so we can hand it on to
    // the queues
    future_queue<void> notifyerQueue(1);

    // copy the list of participating queues
    std::vector<future_queue_base> participants;
    for(auto it = begin; it != end; ++it) participants.push_back(*it);

    // obtain notification queue for any update to any queue
    auto anyNotify = when_any(begin, end);

    // define function to be executed (inside the notifyer queue) on non-blocking
    // functions like pop() or empty()
    notifyerQueue.d->continuation_process_deferred = std::function<void(void)>([notifyerQueue, participants]() mutable {
      bool empty = false;
      for(auto& q : participants) {
        if(q.empty()) {
          empty = true;
          break;
        }
      }
      if(!empty) notifyerQueue.push();
    });

    // define function to be executed (inside the notifyer queue) on blocking
    // functions like pop_wait() or wait()
    notifyerQueue.d->continuation_process_deferred_wait =
        std::function<void(void)>([notifyerQueue, participants, anyNotify]() mutable {
          while(true) {
            anyNotify.pop_wait();
            bool empty = false;
            for(auto& q : participants) {
              if(q.empty()) {
                empty = true;
                break;
              }
            }
            if(!empty) break;
          }
          notifyerQueue.push();
        });

    // set flag marking the notifyerQueue a when_all continuation and save the
    // notification queue of the when_any as the origin.
    notifyerQueue.d->is_continuation_when_all = true;
    notifyerQueue.d->continuation_origin = anyNotify;

    return notifyerQueue;
  }

  /*********************************************************************************************************************/

  namespace detail {

    /** Helper function to realise the data assignment depending on the selected
     * FEATURES tags. The last dummy argument is just used to realise overloads for
     * the different tags (as C++ does not know partial template specialisations
     *  for functions). */
    template<typename T>
    void data_assign(T& a, T&& b, MOVE_DATA) {
      // in order not to depend on the move assignment operator, which might not
      // always be available, we perform an in-place destruction followed by an
      // in-place move construction.
      a.~T();
      new(&a) T(std::move(b));
    }

    template<typename T>
    void data_assign(T& a, T&& b, SWAP_DATA) {
      std::swap(a, b);
    }

  } // namespace detail

  /*********************************************************************************************************************/
  /*********************************************************************************************************************/
  /** Implementation of shared_state_ptr */
  /*********************************************************************************************************************/
  /*********************************************************************************************************************/

  namespace detail {

    inline shared_state_ptr::shared_state_ptr() : ptr(nullptr) {}

    inline shared_state_ptr::shared_state_ptr(const shared_state_ptr& other) {
      // Copy the pointer and increase the reference count
      set(other.get());
      if(get() != nullptr) get()->reference_count++;
    }

    inline shared_state_ptr& shared_state_ptr::operator=(const shared_state_ptr& other) {
      // Free previous target, copy the new pointer and increase its reference count
      free();
      set(other.get());
      if(get() != nullptr) get()->reference_count++;
      return *this;
    }

    inline shared_state_ptr::~shared_state_ptr() { free(); }

    inline shared_state_base* shared_state_ptr::get() const { return ptr; }

    inline void shared_state_ptr::set(shared_state_base* ptr_) { ptr = ptr_; }

    inline void shared_state_ptr::free() {
      // Don't do anything if called on a nullptr (i.e. default constructed or
      // already destroyed)
      if(get() == nullptr) return;

      // Reduce reference count but atomically keep the old reference counter. Note
      // that the std::memory_order_relaxed refers to the access to the pointer not
      // to the reference counter.
      size_t oldCount = get()->reference_count--;

      // Determine whether we need to destroy the shared state depending on possible
      // internal references.
      bool executeDelete = false;

      // Standard case: no continuation. If the last user is just destroying its
      // reference we delete the shared state.
      if(oldCount == 1 && !get()->is_continuation_async && !get()->is_continuation_deferred &&
          !get()->is_continuation_when_all) {
        executeDelete = true;
      }
      // Deferred continuations (incl. when_all) have two internal use counts due to
      // the two std::functions, so we need to remove those functions first.
      else if(oldCount == 3 && (get()->is_continuation_deferred || get()->is_continuation_when_all)) {
        get()->continuation_process_deferred = {};
        get()->continuation_process_deferred_wait = {};
        executeDelete = true;
      }
      // Async continuations have one internal use count inside their thread, so we
      // need to terminate the thread first.
      else if(oldCount == 2 && get()->is_continuation_async) {
        if(get()->continuation_process_async.joinable()) {
          // Signal termination to internal thread and wait until thread has been
          // terminated
          while(get()->continuation_process_async_terminated == false) {
            // Push a detail::TerminateInternalThread exception into the queue which
            // the internal thread is potentially waiting on.
            try {
              throw detail::TerminateInternalThread();
            }
            catch(...) {
              // Special case: the origin queue is a continuation itself (deferred
              // or when_all) - we need to push the exception to the origin of the
              // origin to actually reach the internal thread, since
              // deferred/when_all continuations do not really use their own queue
              if(get()->continuation_origin.d->is_continuation_deferred ||
                  get()->continuation_origin.d->is_continuation_when_all) {
                get()->continuation_origin.d->continuation_origin.push_exception(std::current_exception());
              }
              // Standard case: just push the exception to the origin queue of the
              // continuation
              else {
                get()->continuation_origin.push_exception(std::current_exception());
              }
            }
          }
          get()->continuation_process_async.join();
        }
        executeDelete = true;
      }

      // delete the shared_state?
      if(executeDelete) {
        // Now that all potential internal references have been cleared the
        // reference count must be 0
        assert(get()->reference_count == 0);
        delete get();
        set(nullptr);
      }
    }

    template<typename T>
    void shared_state_ptr::make_new(size_t length) {
      free();
      ptr = new shared_state<T>(length);
      get()->reference_count = 1;
    }

    inline shared_state_base* shared_state_ptr::operator->() {
      assert(get() != nullptr);
      return get();
    }

    inline const shared_state_base* shared_state_ptr::operator->() const {
      assert(get() != nullptr);
      return get();
    }

    template<typename T>
    shared_state<T>* shared_state_ptr::cast() {
      assert(get() != nullptr);
      return static_cast<shared_state<T>*>(get());
    }

    inline shared_state_ptr::operator bool() const { return get() != nullptr; }

    inline bool shared_state_ptr::operator==(const shared_state_ptr& other) const { return get() == other.get(); }

  } // namespace detail

  /*********************************************************************************************************************/
  /*********************************************************************************************************************/
  /** Implementation of future_queue_base */
  /*********************************************************************************************************************/
  /*********************************************************************************************************************/

  inline size_t future_queue_base::write_available() const {
    // Obtain indices in this particular order to ensure consistency. Result might
    // be too small (but not too big) if writing happens concurrently.
    size_t l_writeIndex = d->writeIndex;
    size_t l_readIndex = d->readIndex;
    if(l_writeIndex - l_readIndex < d->nBuffers - 1) {
      return d->nBuffers - (l_writeIndex - l_readIndex) - 1;
    }
    else {
      return 0;
    }
  }

  inline size_t future_queue_base::read_available() const {
    // Single consumer, so atomicity doesn't matter
    return d->readIndexMax - d->readIndex;
  }

  inline cppext::detail::shared_state_base* future_queue_base::get_notification_queue() {
    // if there is no notification queue, atomically increment counter while making sure now notification queue is
    // placed concurrently
    detail::when_any_notification_info info, info_n;
    do {
      info = d->when_any_notification.load(std::memory_order_acquire);
      if(info.notifyerQueue) break;
      info_n = info;
      ++info_n.notifyerQueue_previousData;
    } while(!d->when_any_notification.compare_exchange_weak(info, info_n));
    return info.notifyerQueue;
  }

  inline void future_queue_base::send_notification(cppext::detail::shared_state_base* notification_queue) {
    // if there is a notification queue, push to it
    if(notification_queue) {
      future_queue<size_t, MOVE_DATA> n;
      n.d.set(notification_queue);
      bool nret = n.push(d->when_any_index);
      n.d.set(nullptr); // prevent reference count from being decremented
      (void)nret;
      // This assert doesn't really hold. It might spuriously fail during destruction of certain combinations of
      // continuations and when_any/when_all.
      // assert(nret == true);
    }
  }

  inline void future_queue_base::decrement_previous_data_counter() {
    detail::when_any_notification_info info, info_n;
    do {
      info = d->when_any_notification.load(std::memory_order_acquire);
      if(info.notifyerQueue) break; // no need to deal with this counter if notification queue present
      info_n = info;
      assert(info_n.notifyerQueue_previousData > 0);
      --info_n.notifyerQueue_previousData;
    } while(!d->when_any_notification.compare_exchange_weak(info, info_n));
  }

  inline bool future_queue_base::push_exception(std::exception_ptr exception) {
    // obtain index to write to
    size_t myIndex;
    if(!obtain_write_slot(myIndex)) return false;

    // assign the payload data (data buffer is ignored if exception is set)
    d->exceptions[myIndex % d->nBuffers] = exception;

    // obtain notification queue or increment previous data counter (for when_any)
    auto notification_queue = get_notification_queue();

    // signal receiving end
    assert(!d->semaphores[myIndex % d->nBuffers].is_ready());
    d->semaphores[myIndex % d->nBuffers].unlock();
    update_read_index_max();

    // deal with when_any notifications
    send_notification(notification_queue);
    return true;
  }

  inline bool future_queue_base::push_overwrite_exception(std::exception_ptr exception) {
    assert(d->nBuffers - 1 > 1);
    bool ret = true;

    // obtain index to write to, if necessary remove old data first
    size_t myIndex;
    if(!obtain_write_slot(myIndex)) {
      if(d->semaphores[(myIndex - 1) % d->nBuffers].is_ready_and_reset()) {
        size_t expectedIndex = myIndex;
        bool success = d->writeIndex.compare_exchange_strong(expectedIndex, myIndex - 1);
        if(!success) {
          // in case of a concurrent push_overwrite(), our data effectively just got overwritten by the other thread
          // even before writing it...
          d->semaphores[(myIndex - 1) % d->nBuffers].unlock();
          return false;
        }
        ret = false;
      }
      else {
        return false;
      }
      if(!obtain_write_slot(myIndex)) return false;
    }

    // assign the payload data (data buffer is ignored if exception is set)
    d->exceptions[myIndex % d->nBuffers] = exception;

    // obtain notification queue or increment previous data counter (for when_any) (unless data was overwritten)
    cppext::detail::shared_state_base* notification_queue;
    if(ret) {
      notification_queue = get_notification_queue();
    }

    // obtain notification queue or increment previous data counter (for when_any)
    assert(!d->semaphores[myIndex % d->nBuffers].is_ready());
    d->semaphores[myIndex % d->nBuffers].unlock();
    update_read_index_max();

    // deal with when_any notifications (unless data was overwritten)
    if(ret) {
      send_notification(notification_queue);
    }
    return ret;
  }

  inline bool future_queue_base::empty() {
    if(d->hasFrontOwnership) return false;
    if(d->is_continuation_deferred || d->is_continuation_when_all) d->continuation_process_deferred();
    if(d->semaphores[d->readIndex % d->nBuffers].is_ready_and_reset()) {
      d->hasFrontOwnership = true;
      return false;
    }
    return true;
  }

  inline void future_queue_base::wait() {
    if(d->hasFrontOwnership) return;
    if(d->is_continuation_deferred || d->is_continuation_when_all) d->continuation_process_deferred_wait();
    d->semaphores[d->readIndex % d->nBuffers].wait_and_reset();
    d->hasFrontOwnership = true;
  }

  inline size_t future_queue_base::size() const {
    if(!d->is_continuation_deferred) {
      return d->nBuffers - 1;
    }
    else {
      return d->continuation_origin.size();
    }
  }

  inline bool future_queue_base::operator==(const future_queue_base& other) const { return d == other.d; }

  inline bool future_queue_base::operator!=(const future_queue_base& other) const { return !(d == other.d); }

  inline future_queue_base::future_queue_base(const detail::shared_state_ptr& d_ptr_) : d(d_ptr_) {}

  inline future_queue_base::future_queue_base() : d() {}

  inline bool future_queue_base::obtain_write_slot(size_t& index) {
    index = d->writeIndex;
    while(true) {
      if(index >= d->readIndex + d->nBuffers - 1) return false; // queue is full
      bool success = d->writeIndex.compare_exchange_weak(index, index + 1);
      if(success) break;
    }
    return true;
  }

  inline void future_queue_base::update_read_index_max() {
    size_t l_readIndex = d->readIndex;
    size_t l_writeIndex = d->writeIndex;
    size_t l_readIndexMax = d->readIndexMax;
    if(l_writeIndex >= l_readIndex + d->nBuffers) l_writeIndex = l_readIndex + d->nBuffers - 1;
    size_t newReadIndexMax = l_readIndexMax;
    do {
      for(size_t index = l_readIndexMax; index <= l_writeIndex - 1; ++index) {
        if(!d->semaphores[index % d->nBuffers].is_ready()) break;
        newReadIndexMax = index + 1;
      }
      d->readIndexMax.compare_exchange_weak(l_readIndexMax, newReadIndexMax);
    } while(d->readIndexMax < newReadIndexMax);
  }

  inline size_t future_queue_base::setNotificationQueue(
      future_queue<size_t, MOVE_DATA>& notificationQueue, size_t indexToSend) {
    if(!d->is_continuation_deferred) {
      d->when_any_index = indexToSend;

      // create new info struct with notification queue
      detail::when_any_notification_info info;
      info.notifyerQueue = notificationQueue.d.get();
      info.notifyerQueue_previousData = 0;

      // atomically exchange info struct while making sure it has not been altered at the target in the mean time
      detail::when_any_notification_info info_o;
      do {
        info_o = d->when_any_notification;
      } while(!d->when_any_notification.compare_exchange_weak(info_o, info));

      // artificially increment the reference count of the notification queue, since we have to store a plain pointer
      // in the info struct rather than a shared_state_ptr (which is not trivially copyable).
      info.notifyerQueue->reference_count++;

      return info_o.notifyerQueue_previousData;
    }
    else {
      return d->continuation_origin.setNotificationQueue(notificationQueue, indexToSend);
    }
  }

  /*********************************************************************************************************************/
  /*********************************************************************************************************************/
  /** Implementation of future_queue */
  /*********************************************************************************************************************/
  /*********************************************************************************************************************/

  template<typename T, typename FEATURES>
  future_queue<T, FEATURES>::future_queue(size_t length) : future_queue_base(detail::make_shared_state<T>(length)) {}

  template<typename T, typename FEATURES>
  future_queue<T, FEATURES>::future_queue() {}

  /*********************************************************************************************************************/
  /** Various implementations of push(). */

  /** This push() is for non-void data types passed by Rvalue reference */
  template<typename T, typename FEATURES>
  template<typename U, typename std::enable_if<std::is_same<T, U>::value && !std::is_same<U, void>::value, int>::type>
  bool future_queue<T, FEATURES>::push(U&& t) {
    // obtain index to write to
    size_t myIndex;
    if(!obtain_write_slot(myIndex)) return false;

    // assign the payload data
    detail::data_assign(future_queue_base::d.cast<T>()->buffers[myIndex % d->nBuffers], std::move(t), FEATURES());
    d->exceptions[myIndex % d->nBuffers] = nullptr;

    // obtain notification queue or increment previous data counter (for when_any)
    auto notification_queue = get_notification_queue();

    // signal receiving end
    assert(!d->semaphores[myIndex % d->nBuffers].is_ready());
    d->semaphores[myIndex % d->nBuffers].unlock();
    update_read_index_max(); // basically only for read_available()

    // deal with when_any notifications
    send_notification(notification_queue);
    return true;
  }

  /** This push() is for non-void data types passed by Lvalue reference */
  template<typename T, typename FEATURES>
  template<typename U,
      typename std::enable_if<!std::is_same<U, void>::value && std::is_copy_constructible<T>::value, int>::type>
  bool future_queue<T, FEATURES>::push(const U& t) {
    // Create copy and pass this copy as an Rvalue reference to the other
    // implementation
    return push(T(t));
  }

  /** This push() is for void data type */
  template<typename T, typename FEATURES>
  bool future_queue<T, FEATURES>::push(void) {
    static_assert(
        std::is_same<T, void>::value, "future_queue<T,FEATURES>::push(void) may only be called for T = void.");
    // obtain index to write to
    size_t myIndex;
    if(!obtain_write_slot(myIndex)) return false;

    // assign the payload data
    d->exceptions[myIndex % d->nBuffers] = nullptr;

    // obtain notification queue or increment previous data counter (for when_any)
    auto notification_queue = get_notification_queue();

    // signal receiving end
    assert(!d->semaphores[myIndex % d->nBuffers].is_ready());
    d->semaphores[myIndex % d->nBuffers].unlock();
    update_read_index_max(); // basically only for read_available()

    // deal with when_any notifications
    send_notification(notification_queue);
    return true;
  }

  /** This push_overwrite() is for non-void data types passed by Rvalue reference
   */
  template<typename T, typename FEATURES>
  template<typename U, typename std::enable_if<std::is_same<T, U>::value && !std::is_same<U, void>::value, int>::type>
  bool future_queue<T, FEATURES>::push_overwrite(U&& t) {
    assert(d->nBuffers - 1 > 1);
    bool ret = true;

    // obtain index to write to, if necessary remove old data first
    size_t myIndex;
    if(!obtain_write_slot(myIndex)) {
      if(d->semaphores[(myIndex - 1) % d->nBuffers].is_ready_and_reset()) {
        size_t expectedIndex = myIndex;
        bool success = d->writeIndex.compare_exchange_strong(expectedIndex, myIndex - 1);
        if(!success) {
          // in case of a concurrent push_overwrite(), our data effectively just got overwritten by the other thread
          // even before writing it...
          d->semaphores[(myIndex - 1) % d->nBuffers].unlock();
          return false;
        }
        ret = false;
      }
      else {
        return false;
      }
      if(!obtain_write_slot(myIndex)) return false;
    }

    // assign the payload data
    detail::data_assign(future_queue_base::d.cast<T>()->buffers[myIndex % d->nBuffers], std::move(t), FEATURES());
    d->exceptions[myIndex % d->nBuffers] = nullptr;

    // obtain notification queue or increment previous data counter (for when_any) (unless data was overwritten)
    [[maybe_unused]] cppext::detail::shared_state_base* notification_queue;
    if(ret) {
      notification_queue = get_notification_queue();
    }

    // signal receiving end
    assert(!d->semaphores[myIndex % d->nBuffers].is_ready());
    d->semaphores[myIndex % d->nBuffers].unlock();
    update_read_index_max();

    // deal with when_any notifications (unless data was overwritten)
    if(ret) {
      send_notification(notification_queue);
    }
    return ret;
  }

  /** This push_overwrite() is for non-void data types passed by Lvalue reference
   */
  template<typename T, typename FEATURES>
  template<typename U,
      typename std::enable_if<!std::is_same<U, void>::value && std::is_copy_constructible<T>::value, int>::type>
  bool future_queue<T, FEATURES>::push_overwrite(const U& t) {
    // Create copy and pass this copy as an Rvalue reference to the other
    // implementation
    return push_overwrite(T(t));
  }

  /*********************************************************************************************************************/
  /** Various implementations of pop(). */

  /** This pop() is for non-void data types */
  template<typename T, typename FEATURES>
  template<typename U, typename std::enable_if<std::is_same<T, U>::value && !std::is_same<U, void>::value, int>::type>
  bool future_queue<T, FEATURES>::pop(U& t) {
    if((d->is_continuation_deferred || d->is_continuation_when_all) && !d->hasFrontOwnership) {
      d->continuation_process_deferred();
    }
    if(d->hasFrontOwnership || d->semaphores[d->readIndex % d->nBuffers].is_ready_and_reset()) {
      std::exception_ptr e;
      if(d->exceptions[d->readIndex % d->nBuffers]) {
        e = d->exceptions[d->readIndex % d->nBuffers];
      }
      else {
        detail::data_assign(
            t, std::move(future_queue_base::d.cast<T>()->buffers[d->readIndex % d->nBuffers]), FEATURES());
      }
      assert(d->readIndex < d->writeIndex);
      d->readIndex++;
      d->hasFrontOwnership = false;
      decrement_previous_data_counter();
      if(e) std::rethrow_exception(e);
      return true;
    }
    else {
      return false;
    }
  }

  /** This pop() is for all data types (for non-void data types the value will be
   * discarded) */
  template<typename T, typename FEATURES>
  bool future_queue<T, FEATURES>::pop() {
    if((d->is_continuation_deferred || d->is_continuation_when_all) && !d->hasFrontOwnership) {
      d->continuation_process_deferred();
    }
    if(d->hasFrontOwnership || d->semaphores[d->readIndex % d->nBuffers].is_ready_and_reset()) {
      std::exception_ptr e;
      if(d->exceptions[d->readIndex % d->nBuffers]) {
        e = d->exceptions[d->readIndex % d->nBuffers];
      }
      assert(d->readIndex < d->writeIndex);
      d->readIndex++;
      d->hasFrontOwnership = false;
      decrement_previous_data_counter();
      if(e) std::rethrow_exception(e);
      return true;
    }
    else {
      return false;
    }
  }

  /** This pop_void() is for non-void data types */
  template<typename T, typename FEATURES>
  template<typename U, typename std::enable_if<std::is_same<T, U>::value && !std::is_same<U, void>::value, int>::type>
  void future_queue<T, FEATURES>::pop_wait(U& t) {
    if(!d->hasFrontOwnership) {
      if(d->is_continuation_deferred || d->is_continuation_when_all) d->continuation_process_deferred_wait();
      d->semaphores[d->readIndex % d->nBuffers].wait_and_reset();
    }
    else {
      d->hasFrontOwnership = false;
    }
    std::exception_ptr e;
    if(d->exceptions[d->readIndex % d->nBuffers]) {
      e = d->exceptions[d->readIndex % d->nBuffers];
    }
    else {
      detail::data_assign(
          t, std::move(future_queue_base::d.cast<U>()->buffers[d->readIndex % d->nBuffers]), FEATURES());
    }
    assert(d->readIndex < d->writeIndex);
    d->readIndex++;
    decrement_previous_data_counter();
    if(e) std::rethrow_exception(e);
  }

  /** This pop_wait() is for all data types (for non-void data types the value
   * will be discarded) */
  template<typename T, typename FEATURES>
  void future_queue<T, FEATURES>::pop_wait() {
    if(!d->hasFrontOwnership) {
      if(d->is_continuation_deferred || d->is_continuation_when_all) d->continuation_process_deferred_wait();
      d->semaphores[d->readIndex % d->nBuffers].wait_and_reset();
    }
    else {
      d->hasFrontOwnership = false;
    }
    std::exception_ptr e;
    if(d->exceptions[d->readIndex % d->nBuffers]) {
      e = d->exceptions[d->readIndex % d->nBuffers];
    }
    assert(d->readIndex < d->writeIndex);
    d->readIndex++;
    decrement_previous_data_counter();
    if(e) std::rethrow_exception(e);
  }

  /*********************************************************************************************************************/
  /** Various implementations of front(). */

  /** This front() is for non-void data types and a non-const *this */
  template<typename T, typename FEATURES>
  template<typename U, typename std::enable_if<std::is_same<T, U>::value && !std::is_same<U, void>::value, int>::type>
  U& future_queue<T, FEATURES>::front() {
    assert(d->hasFrontOwnership);
    if(d->exceptions[d->readIndex % d->nBuffers]) std::rethrow_exception(d->exceptions[d->readIndex % d->nBuffers]);
    return future_queue_base::d.cast<T>()->buffers[d->readIndex % d->nBuffers];
  }

  /*********************************************************************************************************************/
  /** Implementation of then() with helper classes. They are implemented as
   * classes since we need partial template specialisations. */

  namespace detail {
    // ----------------------------------------------------------------------------------------------------------------
    // ----------------------------------------------------------------------------------------------------------------
    // helper functions used inside future_queue::then()

    // ----------------------------------------------------------------------------------------------------------------
    // continuation_process_deferred: function to be executed in a deferred
    // continuation in non-blocking functions

    // continuation_process_deferred for non-void data types
    template<typename T, typename FEATURES, typename TOUT, typename CALLABLE>
    struct continuation_process_deferred {
      continuation_process_deferred(
          future_queue<T, FEATURES> q_input_, future_queue<TOUT> q_output_, CALLABLE callable_)
      : q_input(q_input_), q_output(q_output_), callable(callable_) {}
      void operator()() {
        // written this way so the callable is able to swap with the internal buffer
        if(q_input.empty()) return;
        try {
          q_output.push(callable(q_input.front()));
        }
        catch(...) {
          q_output.push_exception(std::current_exception());
        }
        try {
          q_input.pop();
        }
        catch(...) {
          // exception already pushed to the output queue, so ignore here
        }
      }
      future_queue<T, FEATURES> q_input;
      future_queue<TOUT> q_output;
      CALLABLE callable;
    };

    // continuation_process_deferred for void input and non-void output data types
    template<typename FEATURES, typename TOUT, typename CALLABLE>
    struct continuation_process_deferred<void, FEATURES, TOUT, CALLABLE> {
      continuation_process_deferred(
          future_queue<void, FEATURES> q_input_, future_queue<TOUT> q_output_, CALLABLE callable_)
      : q_input(q_input_), q_output(q_output_), callable(callable_) {}
      void operator()() {
        bool got_data = q_input.pop();
        if(got_data) q_output.push(callable());
      }
      future_queue<void, FEATURES> q_input;
      future_queue<TOUT> q_output;
      CALLABLE callable;
    };

    // continuation_process_deferred for non-void input and void output data types
    template<typename T, typename FEATURES, typename CALLABLE>
    struct continuation_process_deferred<T, FEATURES, void, CALLABLE> {
      continuation_process_deferred(
          future_queue<T, FEATURES> q_input_, future_queue<void> q_output_, CALLABLE callable_)
      : q_input(q_input_), q_output(q_output_), callable(callable_) {}
      void operator()() {
        // written this way so the callable is able to swap with the internal buffer
        if(q_input.empty()) return;
        try {
          callable(q_input.front());
          q_output.push();
        }
        catch(...) {
          q_output.push_exception(std::current_exception());
        }
        try {
          q_input.pop();
        }
        catch(...) {
          // exception already pushed to the output queue, so ignore here
        }
      }
      future_queue<T, FEATURES> q_input;
      future_queue<void> q_output;
      CALLABLE callable;
    };

    // continuation_process_deferred for void input and void output data types
    template<typename FEATURES, typename CALLABLE>
    struct continuation_process_deferred<void, FEATURES, void, CALLABLE> {
      continuation_process_deferred(
          future_queue<void, FEATURES> q_input_, future_queue<void> q_output_, CALLABLE callable_)
      : q_input(q_input_), q_output(q_output_), callable(callable_) {}
      void operator()() {
        try {
          bool got_data = q_input.pop();
          if(got_data) {
            callable();
            q_output.push();
          }
        }
        catch(...) {
          q_output.push_exception(std::current_exception());
        }
      }
      future_queue<void, FEATURES> q_input;
      future_queue<void> q_output;
      CALLABLE callable;
    };

    // factory for continuation_process_deferred
    template<typename T, typename FEATURES, typename TOUT, typename CALLABLE>
    continuation_process_deferred<T, FEATURES, TOUT, CALLABLE> make_continuation_process_deferred(
        future_queue<T, FEATURES> q_input, future_queue<TOUT> q_output, CALLABLE callable) {
      return {q_input, q_output, callable};
    }

    // ----------------------------------------------------------------------------------------------------------------
    // continuation_process_deferred_wait: function to be executed in a deferred
    // continuation in blocking functions

    // continuation_process_deferred_wait for non-void data types
    template<typename T, typename FEATURES, typename TOUT, typename CALLABLE>
    struct continuation_process_deferred_wait {
      continuation_process_deferred_wait(
          future_queue<T, FEATURES> q_input_, future_queue<TOUT> q_output_, CALLABLE callable_)
      : q_input(q_input_), q_output(q_output_), callable(callable_) {}
      void operator()() {
        // written this way so the callable is able to swap with the internal buffer
        try {
          q_input.wait();
          q_output.push(callable(q_input.front()));
        }
        catch(...) {
          q_output.push_exception(std::current_exception());
        }
        try {
          q_input.pop();
        }
        catch(...) {
          // exception already pushed to the output queue, so ignore here
        }
      }
      future_queue<T, FEATURES> q_input;
      future_queue<TOUT> q_output;
      CALLABLE callable;
    };

    // continuation_process_deferred_wait for void input and non-void output data
    // types
    template<typename FEATURES, typename TOUT, typename CALLABLE>
    struct continuation_process_deferred_wait<void, FEATURES, TOUT, CALLABLE> {
      continuation_process_deferred_wait(
          future_queue<void, FEATURES> q_input_, future_queue<TOUT> q_output_, CALLABLE callable_)
      : q_input(q_input_), q_output(q_output_), callable(callable_) {}
      void operator()() {
        try {
          q_input.pop_wait();
          q_output.push(callable());
        }
        catch(...) {
          q_output.push_exception(std::current_exception());
        }
      }
      future_queue<void, FEATURES> q_input;
      future_queue<TOUT> q_output;
      CALLABLE callable;
    };

    // continuation_process_deferred_wait for non-void input and void output data
    // types
    template<typename T, typename FEATURES, typename CALLABLE>
    struct continuation_process_deferred_wait<T, FEATURES, void, CALLABLE> {
      continuation_process_deferred_wait(
          future_queue<T, FEATURES> q_input_, future_queue<void> q_output_, CALLABLE callable_)
      : q_input(q_input_), q_output(q_output_), callable(callable_) {}
      void operator()() {
        // written this way so the callable is able to swap with the internal buffer
        try {
          q_input.wait();
          callable(q_input.front());
          q_output.push();
        }
        catch(...) {
          q_output.push_exception(std::current_exception());
        }
        try {
          q_input.pop();
        }
        catch(...) {
          // exception already pushed to the output queue, so ignore here
        }
      }
      future_queue<T, FEATURES> q_input;
      future_queue<void> q_output;
      CALLABLE callable;
    };

    // continuation_process_deferred_wait for void input and void output data types
    template<typename FEATURES, typename CALLABLE>
    struct continuation_process_deferred_wait<void, FEATURES, void, CALLABLE> {
      continuation_process_deferred_wait(
          future_queue<void, FEATURES> q_input_, future_queue<void> q_output_, CALLABLE callable_)
      : q_input(q_input_), q_output(q_output_), callable(callable_) {}
      void operator()() {
        try {
          q_input.pop_wait();
          callable();
          q_output.push();
        }
        catch(...) {
          q_output.push_exception(std::current_exception());
        }
      }
      future_queue<void, FEATURES> q_input;
      future_queue<void> q_output;
      CALLABLE callable;
    };

    // factory for continuation_process_deferred_wait
    template<typename T, typename FEATURES, typename TOUT, typename CALLABLE>
    continuation_process_deferred_wait<T, FEATURES, TOUT, CALLABLE> make_continuation_process_deferred_wait(
        future_queue<T, FEATURES> q_input, future_queue<TOUT> q_output, CALLABLE callable) {
      return {q_input, q_output, callable};
    }

    // ----------------------------------------------------------------------------------------------------------------
    // continuation_process_async: function to be executed in the internal thread of
    // a async continuation

    // continuation_process_async for non-void data types
    template<typename T, typename FEATURES, typename TOUT, typename CALLABLE>
    struct continuation_process_async {
      continuation_process_async(future_queue<T, FEATURES> q_input_, future_queue<TOUT> q_output_, CALLABLE callable_)
      : q_input(q_input_), q_output(q_output_), callable(callable_) {}
      void operator()() {
        while(true) {
          // written this way so the callable is able to swap with the internal
          // buffer
          q_input.wait();
          T* v;
          try {
            v = &(q_input.front());
            // TODO how to handle full output queues?
            q_output.push(callable(*v));
          }
          catch(detail::TerminateInternalThread&) {
            q_output.d->continuation_process_async_terminated = true;
            return;
          }
          catch(...) {
            // TODO how to handle full output queues?
            q_output.push_exception(std::current_exception());
          }
          try {
            q_input.pop();
          }
          catch(...) {
            // exception already pushed to the output queue, so ignore here
          }
        }
      }
      future_queue<T, FEATURES> q_input;
      future_queue<TOUT> q_output;
      CALLABLE callable;
    };

    // continuation_process_async for void input and non-void output data types
    template<typename FEATURES, typename TOUT, typename CALLABLE>
    struct continuation_process_async<void, FEATURES, TOUT, CALLABLE> {
      continuation_process_async(
          future_queue<void, FEATURES> q_input_, future_queue<TOUT> q_output_, CALLABLE callable_)
      : q_input(q_input_), q_output(q_output_), callable(callable_) {}
      void operator()() {
        while(true) {
          try {
            q_input.pop_wait();
            // TODO how to handle full output queues?
            q_output.push(callable());
          }
          catch(detail::TerminateInternalThread&) {
            q_output.d->continuation_process_async_terminated = true;
            return;
          }
          catch(...) {
            // TODO how to handle full output queues?
            q_output.push_exception(std::current_exception());
          }
        }
      }
      future_queue<void, FEATURES> q_input;
      future_queue<TOUT> q_output;
      CALLABLE callable;
    };

    // continuation_process_async for non-void input and void output data types
    template<typename T, typename FEATURES, typename CALLABLE>
    struct continuation_process_async<T, FEATURES, void, CALLABLE> {
      continuation_process_async(future_queue<T, FEATURES> q_input_, future_queue<void> q_output_, CALLABLE callable_)
      : q_input(q_input_), q_output(q_output_), callable(callable_) {}
      void operator()() {
        while(true) {
          // written this way so the callable is able to swap with the internal
          // buffer
          q_input.wait();
          T* v;
          try {
            v = &(q_input.front());
            callable(*v);
            // TODO how to handle full output queues?
            q_output.push();
          }
          catch(detail::TerminateInternalThread&) {
            q_output.d->continuation_process_async_terminated = true;
            return;
          }
          catch(...) {
            // TODO how to handle full output queues?
            q_output.push_exception(std::current_exception());
          }
          try {
            q_input.pop();
          }
          catch(...) {
            // exception already pushed to the output queue, so ignore here
          }
        }
      }
      future_queue<T, FEATURES> q_input;
      future_queue<void> q_output;
      CALLABLE callable;
    };

    // continuation_process_async for void input and void output data types
    template<typename FEATURES, typename CALLABLE>
    struct continuation_process_async<void, FEATURES, void, CALLABLE> {
      continuation_process_async(
          future_queue<void, FEATURES> q_input_, future_queue<void> q_output_, CALLABLE callable_)
      : q_input(q_input_), q_output(q_output_), callable(callable_) {}
      void operator()() {
        while(true) {
          try {
            q_input.pop_wait();
            callable();
            // TODO how to handle full output queues?
            q_output.push();
          }
          catch(detail::TerminateInternalThread&) {
            q_output.d->continuation_process_async_terminated = true;
            return;
          }
          catch(...) {
            // TODO how to handle full output queues?
            q_output.push_exception(std::current_exception());
          }
        }
      }
      future_queue<void, FEATURES> q_input;
      future_queue<void> q_output;
      CALLABLE callable;
    };

    // factory for continuation_process_async
    template<typename T, typename FEATURES, typename TOUT, typename CALLABLE>
    continuation_process_async<T, FEATURES, TOUT, CALLABLE> make_continuation_process_async(
        future_queue<T, FEATURES> q_input, future_queue<TOUT> q_output, CALLABLE callable) {
      return {q_input, q_output, callable};
    }
  } // namespace detail

  // ----------------------------------------------------------------------------------------------------------------
  // ----------------------------------------------------------------------------------------------------------------
  // actual implementation of future_queue::then()

  template<typename T, typename FEATURES>
  template<typename T2, typename FEATURES2, typename CALLABLE>
  future_queue<T2, FEATURES2> future_queue<T, FEATURES>::then(CALLABLE callable, std::launch policy) {
    future_queue<T, FEATURES> q_input(*this);
    if(policy == std::launch::deferred) {
      future_queue<T2, FEATURES2> q_output(1);
      q_output.d->continuation_process_deferred =
          detail::make_continuation_process_deferred(q_input, q_output, callable);
      q_output.d->continuation_process_deferred_wait =
          detail::make_continuation_process_deferred_wait(q_input, q_output, callable);
      q_output.d->continuation_origin = *this;
      q_output.d->is_continuation_deferred = true;
      return q_output;
    }
    else {
      future_queue<T2, FEATURES2> q_output(size());
      q_output.d->continuation_process_async =
          std::thread(detail::make_continuation_process_async(q_input, q_output, callable));
      q_output.d->continuation_origin = *this;
      q_output.d->is_continuation_async = true;
      return q_output;
    }
  }

} // namespace cppext

#endif // FUTURE_QUEUE_HPP
