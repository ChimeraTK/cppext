#define BOOST_TEST_MODULE test_future_queue
#include <boost/test/included/unit_test.hpp>
using namespace boost::unit_test_framework;

#include <random>
#include <boost/thread/thread.hpp>

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

    std::random_device rd;
    std::mt19937 gen(rd());

    // create queues, with each a random length
    std::list<future_queue<int>> qlist;
    std::uniform_int_distribution<> qlength(2, 123);
    for(size_t iq=0; iq<nQueues; ++iq) {
      qlist.emplace_back(qlength(gen));
    }

    // list of threads so we can collect them later
    std::list<boost::thread> senderThreads, receiverThreads;

    // launch receiver threads. They should come first, since when_any() should be executed before the senders start sending.
    auto qit = qlist.begin();
    for(size_t i=0; i<nReceivers; ++i) {

      // build list of queues and next values to send
      std::vector<int> nextValues;
      std::vector<std::reference_wrapper<future_queue<int>>> myqueues;
      std::list<std::reference_wrapper<future_queue_base>> myqueues2;
      std::map<future_queue_base::id_t, std::reference_wrapper<future_queue<int>>> myqueuemap;
      std::map<future_queue_base::id_t, int> nextValues2;
      for(size_t k=0; k<nQueuesPerReceiver; ++k) {
        myqueues.emplace_back(*qit);
        myqueues2.emplace_back(*qit);
        nextValues.push_back(0);
        myqueuemap.emplace(qit->get_id(), *qit);
        nextValues2[qit->get_id()] = 0;
        ++qit;
      }

      int type = i % 2;       // alternate the different receiver types
      if(type == 0) {         // first type: go through all queues and wait on each once
        receiverThreads.emplace_back( [i,nextValues, myqueues, &shutdownReceivers] () mutable {
          // 'endless' loop to send data
          while(!shutdownReceivers) {
            for(size_t k=0; k<nQueuesPerReceiver; ++k) {
              int value;
              myqueues[k].get().pop_wait(value);
              assert(value == nextValues[k]);
              ++nextValues[k];
            }
          }
        } );  // end receiver thread for first type
      }
      else if(type == 1) {         // second type: use when_any
        // obtain notification queue
        auto notifyer = when_any(myqueues2);
        // launch the thread
        receiverThreads.emplace_back( [i,nextValues2, notifyer, myqueuemap, &shutdownReceivers] () mutable {
          // 'endless' loop to send data
          while(!shutdownReceivers) {
            future_queue_base::id_t id;
            notifyer->pop_wait(id);
            int value;
            assert(myqueuemap.at(id).get().has_data());
            bool ret = myqueuemap.at(id).get().pop(value);
            assert(ret);
            assert(value == nextValues2.at(id));
            ++nextValues2.at(id);
          }
        } );  // end receiver thread for first type
      }
    }
    assert(qit == qlist.end());

    // launch sender threads
    qit = qlist.begin();
    for(size_t i=0; i<nSenders; ++i) {

      // build list of queues and next values to send
      std::vector<int> nextValues;
      std::vector<std::reference_wrapper<future_queue<int>>> myqueues;
      for(size_t k=0; k<nQueuesPerSender; ++k) {
        myqueues.emplace_back(*qit);
        ++qit;
        nextValues.push_back(0);
      }

      senderThreads.emplace_back( [nextValues, myqueues, &shutdownSenders] () mutable {
        //std::cout << "Launching sender..." << std::endl;
        // 'endless' loop to send data
        while(!shutdownSenders) {
          for(size_t k=0; k<nQueuesPerSender; ++k) {
            bool success = myqueues[k].get().push(nextValues[k]);
            if(success) ++nextValues[k];
          }
        }
        // send one more value before shutting down, so the receiving side does not hang
        for(size_t k=0; k<nQueuesPerSender; ++k) myqueues[k].get().push(nextValues[k]);
        //std::cout << "Terminating sender..." << std::endl;
      } );  // end sender thread
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
