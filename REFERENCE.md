# future_queue reference documentation

## Synopsis

```C++

template<typename T, typename FEATURES=MOVE_DATA>
class future_queue : public detail::future_queue_base {
  public:
    // construct/copy/destruct
    future_queue(size_t length);
    future_queue();
    future_queue(const future_queue &other) = default;
    future_queue& operator=(const future_queue &other);

    // public member functions
    bool push(T&& t);
    bool push(const T& t);
    bool push_overwrite(T&& t);
    bool push_overwrite(const T& t);
    bool pop(T& t);
    bool pop();
    void pop_wait(T& t);
    void pop_wait();
    bool empty();
    const T& front() const;
    size_t write_available() const;
    size_t read_available() const;
    size_t size() const;

    template<typename T2, typename FEATURES2=MOVE_DATA, typename CALLABLE>
    future_queue<T2,FEATURES2> then(CALLABLE callable, std::launch policy = std::launch::async);
};

// non-member functions:
template<typename ITERATOR_TYPE>
future_queue<size_t> when_any(ITERATOR_TYPE begin, ITERATOR_TYPE end);

template<typename ITERATOR_TYPE>
future_queue<void> when_all(ITERATOR_TYPE begin, ITERATOR_TYPE end);
```

## Description
A multi-producer single-consumer fifo queue of a fixed length with wait-free push and pop operations. The queue also provides a pop operation which will wait until there is data in the queue in case it is empty.

### Policies
The template parameter ```T``` specifies the type of the user data stored in the queue. The optional second template parameter takes one of the feature tags. Currently two options are supported:

 - ```MOVE_DATA``` (default): Type T must have a move constructor. To place objects on the queue and to retrieve them
                        from the queue, a move operation is performed.
 - ```SWAP_DATA```:           The function ```std::swap()``` must be overloaded for the type ```T```. When placing objects on the
                        queue, ```std::swap()``` is called to exchange the new object with the object currently on the
                        internal queue buffer. This allows avoiding unnecessary memory allocations e.g. when
                        storing ```std::vector``` on the queue, especially if all vectors have the same size.

### Requirements
Objects of the type ```T``` must be default constructible. Upon creation of the queue all internal buffers will be filled with default constructed elements. Depending on the chosen policy, ```T``` must also be movable, or ```std::swap()``` must be overloaded for ```T```.

### ```future_queue``` public construct/copy/destruct

1. ```future_queue(size_t length);```

   The length specifies how many objects the queue can contain at a time. Internally, one additional buffer will be
   allocated. All buffers are allocated upon construction, so no dynamic memory allocation is required later.

2. ```future_queue();```

   The default constructor creates only a place holder which can later be assigned with a properly constructed queue.

3. ```future_queue(const future_queue &other);```

   Copy constructor: After copying the object both ```*this``` and the ```other``` object will refer to the same queue.

4. ```future_queue& operator=(const future_queue &other);```

   Copy assignment operator: After the assignment both ```*this``` and the other object will refer to the same queue.

### ```future_queue``` public member functions

1. ```bool push(T&& t);```

   ```bool push(const T& t);```

   Push object ```t``` to the queue. Returns true if successful and false if queue is full.

2. ```bool push_overwrite(T&& t);```

   ```bool push_overwrite(const T& t);```

   Push object ```t``` to the queue. If the queue is full, the last element will be overwritten and false will be returned. If no data had to be overwritten, true is returned. When using this function, the queue must have a length of at least 2. Note: when used in a multi-producer context the behaviour is undefined!

3. ```bool pop(T& t);```

   ```bool pop();```

   Pop object off the queue and store it in ```t```. If no data is available, false is returned.

4. ```void pop_wait(T& t);```

   ```void pop_wait();```

   Pop object off the queue and store it in ```t```. This function will block until data is available.

5. ```bool empty();```

   Check if there is currently no data on the queue. If the queue contains data (i.e. true will be returned), the function will guarantee that this data can be accessed later e.g. thorugh ```front()``` or ```pop()```. This guarantee holds even if the sender uses ```pop_overwrite()```.

6. ```const T& front() const;```

   Obtain the front element of the queue without removing it. It is mandatory to make sure that data is available in the queue by calling ```empty()``` before calling this function.
 
7. ```size_t write_available() const;```

   Number of push operations which can be performed before the queue is full. Note that the result may be inaccurate e.g. in multi-producer contexts.

8. ```size_t read_available() const;```

   Number of pop operations which can be performed before the queue is empty. Note that the result can be inaccurate in case the sender uses ```push_overwrite()```. If a guarantee is required that a readable element is present before accessing it through ```pop()``` or ```front()```, use ```empty()```.

9. ```size_t size() const;```

   Return length of the queue as specified in the constrution.

### non-member functions
1. ```template<typename ITERATOR_TYPE> future_queue<size_t> when_any(ITERATOR_TYPE begin, ITERATOR_TYPE end);```

   This function expects two forward iterators pointing to a region of a container of ```future_queue``` objects. It returns a ```future_queue``` which will receive the index of each queue relative to the iterator ```begin``` when the respective queue has new data available for reading. This way the returned queue can be used to get notified about each data written to any of the queues. The order of the indices in this queue is guaranteed to be in the same order the data has been written to the queues. If the same queue gets written to multiple times its index will be present in the returned queue the same number of times.

   Behaviour is unspecified if, after the call to ```when_any()```, data is popped from one of the participating queues without retreiving its index previously from the returned queue. Behaviour is also unspecified if the same queue is passed to different calls to this function, or occurres multiple times.

   If ```push_overwrite()``` is used on one of the participating queues, the notifications received through the returned queue might be in a different order (i.e. when data is overwritten, the corresponding queue index is not moved to the correct place later in the notfication queue). Also, a notification for a value written to a queue with ```push_overwrite()``` might appear in the notification queue before the value can be retrieved from the data queue. It is therefore recommended to use ```pop_wait()``` to retrieve the values from the data queues if ```push_overwrite()``` is used. Otherwise failed ```pop()``` have to be retried until the data is received.

   If data is already available in the queues before calling ```when_any()```, the appropriate number of notifications are placed in the notifyer queue in arbitrary order.
   
2. ```template<typename ITERATOR_TYPE> future_queue<void> when_all(ITERATOR_TYPE begin, ITERATOR_TYPE end);```

   This function expects two forward iterators pointing to a region of a container of ```future_queue``` objects. It returns a ```future_queue<void>``` which will receive a notification when all of the queues in the region have received a new value.