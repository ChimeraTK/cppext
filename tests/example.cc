/**********************************************************************************************************************
 *
 * VERY IMPORTANT NOTE!
 *
 * Whenever this file is changed, please update the example in the README.md file as well!
 *
 *********************************************************************************************************************/

#include <thread>
#include <future_queue.hpp>
#include <iostream>
#include <unistd.h>

// define future_queue of doubles with a length of 5
cppext::future_queue<double> myQueue(5);

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
