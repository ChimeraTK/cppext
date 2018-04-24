# future_queue
A proof-of-concept implementation of a thread synchronisation primitive for C++ 11. It can be described as a mixture of the concepts of futures and (lockfree) queues.

[![Build Status](https://travis-ci.org/mhier/future_queue.svg?branch=master)](https://travis-ci.org/mhier/future_queue)
[![codecov](https://codecov.io/gh/mhier/future_queue/branch/master/graph/badge.svg)](https://codecov.io/gh/mhier/future_queue)

## Features
The future_queue is a thread-safe queue with the following features:
* Lockfree: all operations like push() and pop() are lockfree. Only exception: a call to pop_wait() on an empty queue.
* pop_wait(): block the calling thread until there is data available on the queue.
* Multi producer: push() may be called from different threads concurrently on the same queue
* Single consumer: pop()/pop_wait() may only be called from a single thread at the same time
* push_overwrite(): Overwrite the last element in the queue if the queue is full (only in single-producer context)
* when_any(): get notified on new data in a given list of future_queues in a well-defined order
* Shared: Instances are copyable, copies are referring to the same queue.
* Continuations as an analogy to continuations of futures. Execute a function/lambda for each new value in the queue and push its result into a new queue. Can have different execution policies: std::launch::async (function/lambda runs in separate thread) or std::launch::deferred (function/lambda runs inside the pop() of the resulting queue).

## Example
```C++
#include <thread>
#include <future_queue.hpp>
#include <iostream>
#include <unistd.h>

// define future_queue of doubles with a length of 5
future_queue<double> myQueue(5);

// define function sending data in a separate thread
void senderThread() {
  for(size_t i=0; i<10; ++i) {
    usleep(100000);             // wait 0.1 second
    myQueue.push(i*3.14);
  }
}

int main() {

  // launch sender thread
  std::thread myThread(&senderThread);

  // receive 10 values and print them
  for(size_t i=0; i<10; ++i) {
    double value;
    myQueue.pop_wait(value);                // wait until new data has arrived
    std::cout << value << std::endl;
  }

  myThread.join();
  return 0;
}
```

## Possible extensions
Some ideas how the future_queue could be extended in future:
* Extend push_overwrite() for multi producer scenarios.
* Make it multi consumer (might be difficult!?)
* Allow switching on and off certain features using template parameters to optimise performance

## Performance of the current implementation
The results of the performance test delivered with this library is as follows (on a Intel(R) Core(TM) i5-2500 @ 3.30 GHz):

| Implementation                                                                                       | Time per transfer |
|---------------------------------------------------------------------------------------------------------------|---------:|
|`future_queue` with spin-waiting `pop()`                                                                       |  0.23 us |
|`future_queue` with `pop_wait()`                                                                               |  0.29 us |
|`future_queue` with `when_any()` (10 queues fed by each one thread)                                            |  0.23 us |
|`future_queue` with `when_any()` (100 queues fed by each one thread)                                           |  0.25 us |
| **Comparison with boost::lockfree based implementations**                                                     |          |
|`boost::lockfree::queue` with spin-waiting `pop()`                                                             |  0.15 us |
|`boost::lockfree::spsc_queue` with spin-waiting `pop()`                                                        | 0.006 us |
|`boost::lockfree::spsc_queue<boost::shared_future>` (equivalent to `future_queue::pop_wait()`)                 |  0.94 us |
|`boost::lockfree::spsc_queue<boost::shared_future>` with `wait_for_any()` (10 queues fed by each one thread)   |  2.14 us |
|`boost::lockfree::spsc_queue<boost::shared_future>` with `wait_for_any()` (100 queues fed by each one thread)  | 18.36 us |

The queues were each 1000 elements long and the elements on the queue were of the type int32_t. The used compiler was a gcc 5.4.0 with full optimisations (-03) and the used BOOST version was 1.58. The operating system was Ubuntu 16.04.

The future_queue is a bit slower than the boost::lockfree::queue. If the functionality of pop_wait() is required, the boost::lockfree equivalent would be a spsc_queue<shared_future> which is around 3 times slower than the future_queue (creating futures involves memory allocation). when_any has (after setting it up) a similar performance as a single future_queue, but the equivalent wait_for_any with spsc_queue<shared_future> is much slower, scales badly with the number of participating queues and does not guarantee the order. (The difference between 10 and 100 participating queues in case of the future_queue with when_any seems to be caused only by statistical fluctuations.)

The source code of the performance test can be found here: [tests/testPerformance.cc](tests/testPerformance.cc).
