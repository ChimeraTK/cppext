#define BOOST_TEST_MODULE test_future_queue
#include <boost/test/included/unit_test.hpp>
using namespace boost::unit_test_framework;

#include <thread>
#include <future>

#include "future_queue.hpp"
#include <boost/lockfree/queue.hpp>
#include <boost/lockfree/spsc_queue.hpp>

constexpr size_t queueLength = 1000;
constexpr size_t nTransfers = 1e6;

BOOST_AUTO_TEST_SUITE(testPerformance)

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(future_queue_spin_wait) {
    std::cout << "Measure performance of future_queue with spin-waiting" << std::endl;

    future_queue<int32_t> theQueue(queueLength);

    auto start = std::chrono::steady_clock::now();

    std::thread sender( [&theQueue] {
      for(size_t i=0; i<nTransfers; ++i) {
        while(theQueue.push(i & 0xFFFF) == false) continue;
      }
    } );    // end thread sender

   for(size_t i=0; i<nTransfers; ++i) {
     int32_t val;
     while(theQueue.pop(val) == false) continue;
   }

   auto end = std::chrono::steady_clock::now();
   std::chrono::duration<double> diff = end-start;
   std::cout << "Time for " << nTransfers << " transfers: " << diff.count() << " s\n";
   std::cout << "Average time per transfer: " << diff.count()/(double)nTransfers * 1e6 << " us\n";


   sender.join();

}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(boost_queue_spin_wait) {
    std::cout << "Measure performance of boost::lockfree::queue" << std::endl;

    boost::lockfree::queue<int32_t> theQueue(queueLength);

    auto start = std::chrono::steady_clock::now();

    std::thread sender( [&theQueue] {
      for(size_t i=0; i<nTransfers; ++i) {
        while(theQueue.push(i & 0xFFFF) == false) continue;
      }
    } );    // end thread sender

   for(size_t i=0; i<nTransfers; ++i) {
     int32_t val;
     while(theQueue.pop(val) == false) continue;
   }

   auto end = std::chrono::steady_clock::now();
   std::chrono::duration<double> diff = end-start;
   std::cout << "Time for " << nTransfers << " transfers: " << diff.count() << " s\n";
   std::cout << "Average time per transfer: " << diff.count()/(double)nTransfers * 1e6 << " us\n";


   sender.join();

}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(boost_spsc_queue_spin_wait) {
    std::cout << "Measure performance of boost::lockfree::spsc_queue" << std::endl;

    boost::lockfree::spsc_queue<int32_t> theQueue(queueLength);

    auto start = std::chrono::steady_clock::now();

    std::thread sender( [&theQueue] {
      for(size_t i=0; i<nTransfers; ++i) {
        while(theQueue.push(i & 0xFFFF) == false) continue;
      }
    } );    // end thread sender

   for(size_t i=0; i<nTransfers; ++i) {
     int32_t val;
     while(theQueue.pop(val) == false) continue;
   }

   auto end = std::chrono::steady_clock::now();
   std::chrono::duration<double> diff = end-start;
   std::cout << "Time for " << nTransfers << " transfers: " << diff.count() << " s\n";
   std::cout << "Average time per transfer: " << diff.count()/(double)nTransfers * 1e6 << " us\n";


   sender.join();

}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(future_queue_pop_wait) {
    std::cout << "Measure performance of future_queue with pop_wait" << std::endl;

    future_queue<int32_t> theQueue(queueLength);

    auto start = std::chrono::steady_clock::now();

    std::thread sender( [&theQueue] {
      for(size_t i=0; i<nTransfers; ++i) {
        while(theQueue.push(i & 0xFFFF) == false) continue;
      }
    } );    // end thread sender

   for(size_t i=0; i<nTransfers; ++i) {
     int32_t val;
     theQueue.pop_wait(val);
   }

   auto end = std::chrono::steady_clock::now();
   std::chrono::duration<double> diff = end-start;
   std::cout << "Time for " << nTransfers << " transfers: " << diff.count() << " s\n";
   std::cout << "Average time per transfer: " << diff.count()/(double)nTransfers * 1e6 << " us\n";


   sender.join();

}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(boost_spsc_queue_of_futures) {
    std::cout << "Measure performance of boost::lockfree::spsc_queue<std::shared_future<T>>" << std::endl;

    boost::lockfree::spsc_queue<std::shared_future<int32_t>> theQueue(queueLength);
    std::promise<int32_t> thePromise;
    theQueue.push(thePromise.get_future().share());

    auto start = std::chrono::steady_clock::now();

    std::thread sender( [&theQueue, &thePromise] {
      for(size_t i=0; i<nTransfers; ++i) {
        std::promise<int32_t> newPromise;
        auto newFuture = newPromise.get_future().share();
        while(theQueue.push(newFuture) == false) continue;
        thePromise.set_value(i & 0xFFFF);
        thePromise = std::move(newPromise);
      }
    } );    // end thread sender

   for(size_t i=0; i<nTransfers; ++i) {
     std::shared_future<int32_t> theFuture;
     theQueue.pop(theFuture);
     theFuture.get();
   }

   auto end = std::chrono::steady_clock::now();
   std::chrono::duration<double> diff = end-start;
   std::cout << "Time for " << nTransfers << " transfers: " << diff.count() << " s\n";
   std::cout << "Average time per transfer: " << diff.count()/(double)nTransfers * 1e6 << " us\n";


   sender.join();

}
/*********************************************************************************************************************/

BOOST_AUTO_TEST_SUITE_END()
