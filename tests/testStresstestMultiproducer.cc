// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#define BOOST_TEST_MODULE test_future_queue
#include <boost/test/included/unit_test.hpp>
using namespace boost::unit_test_framework;

#include "future_queue.hpp"
#include "threadsafe_unit_test.hpp"

#include <boost/thread/thread.hpp>

BOOST_AUTO_TEST_SUITE(testStresstestultiproducer)

/**********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(stresstestMultiproducer) {
  constexpr size_t runForSeconds = 10;
  constexpr size_t nSenders = 200;
  constexpr size_t lengthOfQueue = 10;
  constexpr int idsPerSender = 1000;

  std::atomic<bool> shutdownSenders, shutdownReceiver;
  shutdownSenders = false;
  shutdownReceiver = false;

  // create the queue
  cppext::future_queue<int> q(lengthOfQueue);

  // list of threads so we can collect them later
  std::list<boost::thread> senderThreads;

  // launch sender threads
  for(size_t i = 0; i < nSenders; ++i) {
    senderThreads.emplace_back([i, q, &shutdownSenders]() mutable {
      int senderFirstValue = i * idsPerSender;
      // 'endless' loop to send data
      size_t consequtive_fails = 0;
      int nextValue = senderFirstValue;
      while(!shutdownSenders) {
        bool success = q.push(nextValue);
        if(success) {
          ++nextValue;
          if(nextValue >= senderFirstValue + idsPerSender) nextValue = senderFirstValue;
          consequtive_fails = 0;
        }
        else {
          ++consequtive_fails;
          BOOST_CHECK_TS(consequtive_fails < 1000);
          if(consequtive_fails > 100) usleep(100000);
        }
      }
    }); // end sender thread
  }

  // launch receiver thread
  boost::thread receiverThread([q, &shutdownReceiver]() mutable {
    std::vector<int> nextValues(nSenders);
    for(size_t i = 0; i < nSenders; ++i) nextValues[i] = i * idsPerSender;

    // 'endless' loop to receive data
    while(!shutdownReceiver) {
      int value;
      q.pop_wait(value);
      size_t senderId = value / idsPerSender;
      BOOST_CHECK_TS(senderId < nSenders);
      BOOST_CHECK_EQUAL_TS(value, nextValues[senderId]);
      nextValues[senderId]++;
      if(nextValues[senderId] >= ((signed)senderId + 1) * idsPerSender) nextValues[senderId] = senderId * idsPerSender;
    }
  }); // end receiver thread

  // run the test for N seconds
  std::cout << "Keep the test running for " << runForSeconds << " seconds..." << std::endl;
  sleep(runForSeconds);

  // Shutdown all threads and join them. It is important do to that before the
  // queues get destroyed.
  std::cout << "Terminate all threads..." << std::endl;
  shutdownReceiver = true;
  receiverThread.join();
  std::cout << "Receiver thread terminated." << std::endl;
  shutdownSenders = true;
  for(auto& t : senderThreads) t.join();
  std::cout << "All senders are terminated." << std::endl;
}

/**********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(stresstestMultiproducerOverwrite) {
  constexpr size_t runForSeconds = 10;
  constexpr size_t nSenders = 200;
  constexpr size_t lengthOfQueue = 10;
  constexpr int idsPerSender = 1000;

  std::atomic<bool> shutdownSenders, shutdownReceiver;
  shutdownSenders = false;
  shutdownReceiver = false;

  // create the queue
  cppext::future_queue<int> q(lengthOfQueue);

  // list of threads so we can collect them later
  std::list<boost::thread> senderThreads;

  // launch sender threads
  for(size_t i = 0; i < nSenders; ++i) {
    senderThreads.emplace_back([i, q, &shutdownSenders]() mutable {
      int senderFirstValue = i * idsPerSender;
      // 'endless' loop to send data
      size_t consequtive_fails = 0;
      int nextValue = senderFirstValue;
      while(!shutdownSenders) {
        bool success = q.push_overwrite(nextValue);
        ++nextValue;
        if(nextValue >= senderFirstValue + idsPerSender) nextValue = senderFirstValue;
        if(success) {
          consequtive_fails = 0;
        }
        else {
          ++consequtive_fails;
          BOOST_CHECK_TS(consequtive_fails < 1000);
          if(consequtive_fails > 100) usleep(100000);
        }
      }
    }); // end sender thread
  }

  // launch receiver thread
  boost::thread receiverThread([q, &shutdownReceiver]() mutable {
    // 'endless' loop to receive data
    while(!shutdownReceiver) {
      int value;
      q.pop_wait(value);
      size_t senderId = value / idsPerSender;
      assert(senderId < nSenders);
      (void)senderId; // avoid warning in Release builds
    }
  }); // end receiver thread

  // run the test for N seconds
  std::cout << "Keep the test running for " << runForSeconds << " seconds..." << std::endl;
  sleep(runForSeconds);

  // Shutdown all threads and join them. It is important do to that before the
  // queues get destroyed.
  std::cout << "Terminate all threads..." << std::endl;
  shutdownReceiver = true;
  receiverThread.join();
  std::cout << "Receiver thread terminated." << std::endl;
  shutdownSenders = true;
  for(auto& t : senderThreads) t.join();
  std::cout << "All senders are terminated." << std::endl;
}

/**********************************************************************************************************************/

BOOST_AUTO_TEST_SUITE_END()
