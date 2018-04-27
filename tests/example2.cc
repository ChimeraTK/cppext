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
