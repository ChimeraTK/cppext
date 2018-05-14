# future_queue
A proof-of-concept implementation of a thread synchronisation primitive for C++ 11. It can be described as a mixture of the concepts of futures and (lockfree) queues.

For the reference documentation, see [future_queue_reference.md](future_queue_reference.md)

## Features
The future_queue is a thread-safe queue with the following features:
* Lockfree: all operations like push() and pop() are lockfree. Only exception: a call to pop_wait() on an empty queue.
* pop_wait(): block the calling thread until there is data available on the queue.
* Multi producer: push() may be called from different threads concurrently on the same queue
* Single consumer: pop()/pop_wait() may only be called from a single thread at the same time
* push_overwrite(): Overwrite the last element in the queue if the queue is full (only in single-producer context)
* Allows placing exceptions on the queue (cf. std::promise::set_exception())
* when_any(): get notified on new data of any future_queue objects  n a given list, in a well-defined order
* when_all(): get notified on new data of all future_queue objects in a given list
* Continuations as an analogy to continuations of futures. Execute a function/lambda for each new value in the queue and push its result into a new queue. Can have different execution policies: std::launch::async (function/lambda runs in separate thread) or std::launch::deferred (function/lambda runs inside the pop() of the resulting queue).
* Shared: Instances are copyable, copies are referring to the same queue.

## Example
```C++
#include <thread>
#include <future_queue.hpp>
#include <iostream>
#include <unistd.h>

// define future_queue of doubles with a length of 5
static cppext::future_queue<double> myQueue(5);

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

## More advanced example with continuations, ```when_all()``` and ```when_any()```
```C++
#include <thread>
#include <future_queue.hpp>
#include <iostream>
#include <unistd.h>

// define 3 future_queue of int with a length of 5
static cppext::future_queue<int> inputQueue1(5);
static cppext::future_queue<int> inputQueue2(5);
static cppext::future_queue<std::string> inputQueue3(5);

// define functions sending data in separate threads
void senderThread1() {
  for(int i=0; i<10; ++i) {
    usleep(100000);             // wait 0.1 second
    inputQueue1.push(i);
  }
}
void senderThread2() {
  for(int i=20; i<30; ++i) {
    inputQueue2.push(i);
  }
}
void senderThread3() {
  for(int i=20; i<30; ++i) {
    usleep(100000);             // wait 0.1 second
    inputQueue3.push("Value "+std::to_string(i));
  }
}

int main() {

    // setup continuations
    std::vector<cppext::future_queue<double>> temp;   // we should have a convenience function to avoid needing a temporary container...
    temp.push_back(inputQueue1.then<double>( [] (int x) { return x/2.; } ));
    temp.push_back(inputQueue2.then<double>( [] (int x) { return x*3.; } ));

    // process both results of the continuations together
    auto when12 = cppext::when_all(temp.begin(), temp.end());
    auto result12 = when12.then<double>( [temp] () mutable {
      double x,y;
      temp[0].pop(x);
      temp[1].pop(y);
      return x+y;
    } );

    // launch sender threads
    std::thread myThread1(&senderThread1);
    std::thread myThread2(&senderThread2);
    std::thread myThread3(&senderThread3);

    // create notification queue when any result is ready
    std::vector<cppext::future_queue_base> temp2;
    temp2.push_back(result12);
    temp2.push_back(inputQueue3);
    auto ready = cppext::when_any(temp2.begin(), temp2.end());

    // receive 10 values and print them
    for(size_t i=0; i<10; ++i) {
      size_t index;
      ready.pop_wait(index);
      if(index == 0) {
        double value;
        result12.pop(value);
        std::cout << value << std::endl;
      }
      else {
        std::string value;
        inputQueue3.pop(value);
        std::cout << value << std::endl;
      }
    }

    myThread1.join();
    myThread2.join();
    myThread3.join();
    return 0;
}
```
## Known issues
* Bad things will happen if more than std::numeric_limits<size_t>::max write operations have been performed in total, since the internal indices will then overrun. This can be fixed by smarter index handling.
* Asynchronous continuations launch an internal thread which will not terminate (until the main routine terminates). Some properly working shutdown mechanism has to be found.
* The "output" queue in an asynchronous continuation might overrun, which is not handled. At least different handling policies should be defined.
* read_available() and write_available() are unreliable in some situations. Remove them all together?

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
