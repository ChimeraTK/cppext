# future_queue
A proof-of-concept implementation of a thread synchronisation primitive for C++. It can be described as a mixture of the concepts of futures and (lockfree) queues.

## Features
The future_queue is a thread-safe queue with the following features:
* Lockfree: all operations like push() and pop() are lockfree. Only exception: pop_wait().
* pop_wait(): block the calling thread until there is data available on the queue.
* Multi producer: push() may be called from different threads concurrently on the same queue
* Single consumer: pop()/pop_wait() may only be called from a single thread at the same time
* push_overwrite(): Overwrite the last element in the queue if the queue is full (only in single-producer context)
* when_any(): get notified on new data in a given list of future_queues in a well-defined order

## Example
Definition:
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
* Continuations as an analogy to continuations in futures. Execute a function/lambda for each new value in the queue and push its result into a new queue. Might have different execution policies like async (function/lambda runs in separate thread) or lazy (function/lambda runs inside the pop() of the resulting queue).
* Extend push_overwrite() for multi producer scenarios.
* Make it multi consumer (might be difficult!?)
* Allow switching on and off certain features using template parameters to optimise performance
* Allow calling when_any on queues which already contain elements (i.e. fill the notification queue with elements previously on the participating queues - the order will not be defined for those elements)

## Performance of the current implementation
The results of the performance test delivered with this library is as follows (on a Intel(R) Core(TM) i5-2500 @ 3.30 GHz):
<table>
  <tr><th>Implementation</th><th>Time per transfer</th></tr>
  <tr><td>boost::lockfree::queue with spin-waiting pop()</td><td>1.15 us</td></tr>
  <tr><td>boost::lockfree::spsc_queue with spin-waiting pop()</td><td>0.20 us</td></tr>
  <tr><td>future_queue with spin-waiting pop()</td><td>0.78 us</td></tr>
  <tr><td>future_queue with pop_wait()</td><td>0.64 us</td></tr>
  <tr><td>boost::lockfree::spsc_queue<boost::shared_future></td><td>3.90 us</td></tr>
  <tr><td>future_queue with when_any (10 queues fed by each one thread)</td><td>1.36 us</td></tr>
  <tr><td>future_queue with when_any (100 queues fed by each one thread)</td><td>1.42 us</td></tr>
  <tr><td>boost::lockfree::spsc_queue<boost::shared_future> with wait_for_any (10 queues fed by each one thread)</td><td>11.23 us</td></tr>
  <tr><td>boost::lockfree::spsc_queue<boost::shared_future> with wait_for_any (100 queues fed by each one thread)</td><td>75.06 us</td></tr>
</table>
The queues were each 1000 elements long and the elements on the queue were of the type int32_t. The used compiler was a gcc 5.4.0 and the used BOOST version was 1.58. The operating system was Ubuntu 16.04.

As expected, the boost::lockfree::spsc_queue performs best if spin-waiting is acceptable. The performance of the future_queue is somewhere in the middle between the boost::lockfree::queue and the boost::lockfree::spsc_queue. If pop_wait() is required, the boost::lockfree equivalent would be a spsc_queue<shared_future> which is around 5 times slower than the future_queue (creating futures involves memory allocation). when_any is (after setting it up) half as fast as a single future_queue (since each transfer actually consists of two transfers then), but the equivalent wait_for_any with spsc_queue<shared_future> is much slower, scales badly with the number of participating queues and does not guarantee the order. (The difference between 10 and 100 participating queues in case of the future_queue with when_any seems to be caused only by statistical fluctuations.)
