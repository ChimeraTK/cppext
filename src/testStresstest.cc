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

    std::atomic<bool> shutdown;

    std::random_device rd;
    std::mt19937 gen(rd());

    // create queues, with each a random length
    std::list<future_queue<int>> qlist;
    std::uniform_int_distribution<> qlength(2, 123);
    for(size_t iq=0; iq<nQueues; ++iq) {
      qlist.emplace_back(qlength(gen));
    }

    // list of threads so we can collect them later
    std::list<boost::thread> myThreads;

    // launch sender threads
    auto qit = qlist.begin();
    for(size_t i=0; i<nSenders; ++i) {

      // build list of queues and next values to send
      std::vector<int> nextValues;
      std::vector<std::reference_wrapper<future_queue<int>>> myqueues;
      for(size_t k=0; k<nQueuesPerSender; ++k) {
        myqueues.emplace_back(*qit);
        ++qit;
        nextValues.push_back(0);
      }

      myThreads.emplace_back( [nextValues, myqueues, &shutdown] () mutable {
        // 'endless' loop to send data
        while(!shutdown) {
          for(size_t k=0; k<nQueuesPerSender; ++k) {
            bool success = myqueues[k].get().push(nextValues[k]);
            if(success) ++nextValues[k];
          }
        }
      } );  // end sender thread
    }
    assert(qit == qlist.end());

    // launch receiver threads
    qit = qlist.begin();
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
        myThreads.emplace_back( [nextValues, myqueues, &shutdown] () mutable {
          // 'endless' loop to send data
          while(!shutdown) {
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
        myThreads.emplace_back( [nextValues2, myqueues2, myqueuemap, &shutdown] () mutable {
          // obtain notification queue
          auto notifyer = when_any(myqueues2);
          // 'endless' loop to send data
          while(!shutdown) {
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

    // run the test for 30 seconds
    std::cout << "Keep the test running for " << runForSeconds << " seconds..." << std::endl;
    sleep(runForSeconds);

    // Shutdown all threads and join them. It is important do to that before the queues get destroyed.
    std::cout << "Terminate all threads..." << std::endl;
    shutdown = true;
    for(auto &t : myThreads) {
      pthread_kill(t.native_handle(), SIGINT);
      t.join();
    }
    std::cout << "All threads are terminated." << std::endl;

}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_SUITE_END()
