#define BOOST_TEST_MODULE test_future_queue
#include <boost/test/included/unit_test.hpp>
using namespace boost::unit_test_framework;

#include <random>
#include <boost/thread/thread.hpp>

#include "threadsafe_unit_test.hpp"

#include "future_queue.hpp"

BOOST_AUTO_TEST_SUITE(testStresstest)

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(stresstest) {

    constexpr size_t runForSeconds = 10;
    constexpr size_t nQueues = 500;
    constexpr size_t nQueuesPerSender = 20;
    constexpr size_t nQueuesPerReceiver = 50;

    static_assert(nQueues % nQueuesPerSender == 0, "nQueues and nQueuesPerSender do not fit together");
    static_assert(nQueues % nQueuesPerReceiver == 0, "nQueues and nQueuesPerReceiver do not fit together");
    constexpr size_t nSenders = nQueues / nQueuesPerSender;
    constexpr size_t nReceivers = nQueues / nQueuesPerReceiver;

    std::atomic<bool> shutdownSenders, shutdownReceivers;
    shutdownSenders = false;
    shutdownReceivers = false;

    std::random_device rd;
    std::mt19937 gen(rd());

    // create queues, with each a random length
    std::list<cppext::future_queue<int>> qlist;
    std::uniform_int_distribution<> qlength(2, 123);
    for(size_t iq=0; iq<nQueues; ++iq) {
      qlist.emplace_back(qlength(gen));
    }

    // list of threads so we can collect them later
    std::list<boost::thread> senderThreads, receiverThreads;

    // launch sender threads
    auto qit = qlist.begin();
    for(size_t i=0; i<nSenders; ++i) {

      // build list of queues
      std::vector<cppext::future_queue<int>> myqueues;
      for(size_t k=0; k<nQueuesPerSender; ++k) {
        myqueues.emplace_back(*qit);
        ++qit;
      }

      senderThreads.emplace_back( [myqueues, &shutdownSenders] () mutable {
        std::vector<int> nextValues;
        for(size_t i=0; i<myqueues.size(); ++i) nextValues.push_back(0);
        // 'endless' loop to send data
        std::vector<size_t> consequtive_fails(nQueuesPerSender);
        while(!shutdownSenders) {
          for(size_t k=0; k<nQueuesPerSender; ++k) {
            bool success = myqueues[k].push(nextValues[k]);
            if(success) {
              ++nextValues[k];
              consequtive_fails[k] = 0;
            }
            else {
              ++consequtive_fails[k];
              assert(consequtive_fails[k] < 1000);
              if(consequtive_fails[k] > 100) usleep(1000);
            }
          }
        }
        // send one more value before shutting down, so the receiving side does not hang
        for(size_t k=0; k<nQueuesPerSender; ++k) myqueues[k].push(nextValues[k]);
      } );  // end sender thread
    }
    assert(qit == qlist.end());

    // launch receiver threads
    qit = qlist.begin();
    for(size_t i=0; i<nReceivers; ++i) {

      // build list of queues and next values to send
      std::vector<cppext::future_queue<int>> myqueues;
      for(size_t k=0; k<nQueuesPerReceiver; ++k) {
        myqueues.emplace_back(*qit);
        ++qit;
      }

      int type = i % 2;       // alternate the different receiver types
      if(type == 0) {         // first type: go through all queues and wait on each once
        receiverThreads.emplace_back( [myqueues, &shutdownReceivers] () mutable {
          std::vector<int> nextValues;
          for(size_t i=0; i<myqueues.size(); ++i) nextValues.push_back(0);
          // 'endless' loop to send data
          while(!shutdownReceivers) {
            for(size_t k=0; k<nQueuesPerReceiver; ++k) {
              int value;
              myqueues[k].pop_wait(value);
              assert(value == nextValues[k]);
              ++nextValues[k];
            }
          }
        } );  // end receiver thread for first type
      }
      else if(type == 1) {         // second type: use when_any
        // launch the thread
        receiverThreads.emplace_back( [myqueues, &shutdownReceivers] () mutable {
          std::vector<int> nextValues;
          for(size_t i=0; i<myqueues.size(); ++i) nextValues.push_back(0);
          // obtain notification queue
          auto notifyer = when_any(myqueues.begin(), myqueues.end());
          // 'endless' loop to send data
          while(!shutdownReceivers) {
            size_t id;
            notifyer.pop_wait(id);
            int value;
            assert(myqueues[id].empty() == false);
            bool ret = myqueues[id].pop(value);
            (void)ret;
            assert(ret);
            assert(value == nextValues[id]);
            ++nextValues[id];
          }
        } );  // end receiver thread for first type
      }
    }
    assert(qit == qlist.end());

    // run the test for N seconds
    std::cout << "Keep the test running for " << runForSeconds << " seconds..." << std::endl;
    sleep(runForSeconds);

    // Shutdown all threads and join them. It is important do to that before the queues get destroyed.
    std::cout << "Terminate all threads..." << std::endl;
    shutdownReceivers = true;
    for(auto &t : receiverThreads) t.join();
    std::cout << "All receivers are terminated." << std::endl;
    shutdownSenders = true;
    for(auto &t : senderThreads) t.join();
    std::cout << "All senders are terminated." << std::endl;

}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_SUITE_END()
