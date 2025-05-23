// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
/**********************************************************************************************************************
 *
 * VERY IMPORTANT NOTE!
 *
 * Whenever this file is changed, please update the example in the README.md
 *file as well!
 *
 *********************************************************************************************************************/

#include <future_queue.hpp>
#include <unistd.h>

#include <iostream>
#include <thread>

// define future_queue of doubles with a length of 5
static cppext::future_queue<double> myQueue(5);

// define function sending data in a separate thread
void senderThread() {
  for(size_t i = 0; i < 10; ++i) {
    usleep(100000); // wait 0.1 second
    myQueue.push(i * 3.14);
  }
}

int main() {
  // launch sender thread
  std::thread myThread(&senderThread);

  // receive 10 values and print them
  for(size_t i = 0; i < 10; ++i) {
    double value;
    myQueue.pop_wait(value); // wait until new data has arrived
    std::cout << value << std::endl;
  }

  myThread.join();
  return 0;
}
