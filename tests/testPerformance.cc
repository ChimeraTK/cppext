#define BOOST_TEST_MODULE test_future_queue
#include <boost/test/included/unit_test.hpp>
using namespace boost::unit_test_framework;

#include <thread>
#include <iterator>

#include <boost/thread/future.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/lockfree/spsc_queue.hpp>

#include "future_queue.hpp"
#include "barrier.hpp"

constexpr size_t queueLength = 1000;
constexpr size_t nTransfers = 1e6;
constexpr size_t nQueues = 10;        // only for when_any & related
//#define ENABLE_BOOST_LOCKFREE_QUEUE_PERFORMANCE_MEASUREMENT     // just for comparison

/*********************************************************************************************************************/

class helper_iterator {
  public:
    helper_iterator(std::list<std::unique_ptr<boost::lockfree::spsc_queue<boost::shared_future<int32_t>>>>::iterator _it)
    : it(_it) {}

    helper_iterator operator++(int) {
      ++it;
      return *this;
    }

    helper_iterator operator++() {
      auto rval = *this;
      ++it;
      return rval;
    }

    boost::shared_future<int32_t>& operator*() {
      std::unique_ptr<boost::lockfree::spsc_queue<boost::shared_future<int32_t>>> &q = *it;
      return q->front();
    }

    bool operator!=(const helper_iterator &other) const {
      return it != other.it;
    }

    bool operator==(const helper_iterator &other) const {
      return it == other.it;
    }

    std::unique_ptr<boost::lockfree::spsc_queue<boost::shared_future<int32_t>>>& get_queue() {
      return *it;
    }

  private:
    std::list<std::unique_ptr<boost::lockfree::spsc_queue<boost::shared_future<int32_t>>>>::iterator it;
};

namespace std {
  template<>
  struct iterator_traits<helper_iterator> {
      typedef boost::shared_future<int32_t> value_type;
      typedef size_t difference_type;
      typedef std::forward_iterator_tag iterator_category;
  };
}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_SUITE(testPerformance)

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(future_queue_spin_wait) {
    std::cout << "Measure performance of future_queue with spin-waiting" << std::endl;

    future_queue<int32_t> theQueue(queueLength);

    auto start = std::chrono::steady_clock::now();

    std::thread sender( [&theQueue] {
      for(size_t i=0; i<nTransfers; ++i) {
        while(theQueue.push(i & 0xFFFF) == false) usleep(1);
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

#ifdef ENABLE_BOOST_LOCKFREE_QUEUE_PERFORMANCE_MEASUREMENT

BOOST_AUTO_TEST_CASE(boost_queue_spin_wait) {
    std::cout << "Measure performance of boost::lockfree::queue" << std::endl;

    boost::lockfree::queue<int32_t> theQueue(queueLength);

    auto start = std::chrono::steady_clock::now();

    std::thread sender( [&theQueue] {
      for(size_t i=0; i<nTransfers; ++i) {
        while(theQueue.push(i & 0xFFFF) == false) usleep(1);
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

#endif

/*********************************************************************************************************************/

#ifdef ENABLE_BOOST_LOCKFREE_QUEUE_PERFORMANCE_MEASUREMENT

BOOST_AUTO_TEST_CASE(boost_spsc_queue_spin_wait) {
    std::cout << "Measure performance of boost::lockfree::spsc_queue" << std::endl;

    boost::lockfree::spsc_queue<int32_t> theQueue(queueLength);

    auto start = std::chrono::steady_clock::now();

    std::thread sender( [&theQueue] {
      for(size_t i=0; i<nTransfers; ++i) {
        while(theQueue.push(i & 0xFFFF) == false) usleep(1);
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

#endif

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(future_queue_pop_wait) {
    std::cout << "Measure performance of future_queue with pop_wait" << std::endl;

    future_queue<int32_t> theQueue(queueLength);

    auto start = std::chrono::steady_clock::now();

    std::thread sender( [&theQueue] {
      for(size_t i=0; i<nTransfers; ++i) {
        while(theQueue.push(i & 0xFFFF) == false) usleep(1);
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

#ifdef ENABLE_BOOST_LOCKFREE_QUEUE_PERFORMANCE_MEASUREMENT

BOOST_AUTO_TEST_CASE(boost_spsc_queue_of_futures) {
    std::cout << "Measure performance of boost::lockfree::spsc_queue<std::shared_future<T>>" << std::endl;

    boost::lockfree::spsc_queue<boost::shared_future<int32_t>> theQueue(queueLength);
    boost::promise<int32_t> thePromise;
    theQueue.push(thePromise.get_future().share());

    auto start = std::chrono::steady_clock::now();

    std::thread sender( [&theQueue, &thePromise] {
      for(size_t i=0; i<nTransfers; ++i) {
        boost::promise<int32_t> newPromise;
        auto newFuture = newPromise.get_future().share();
        while(theQueue.push(newFuture) == false) usleep(1);
        thePromise.set_value(i & 0xFFFF);
        thePromise = std::move(newPromise);
      }
    } );    // end thread sender

   for(size_t i=0; i<nTransfers; ++i) {
     boost::shared_future<int32_t> theFuture;
     theQueue.pop(theFuture);
     theFuture.get();
   }

   auto end = std::chrono::steady_clock::now();
   std::chrono::duration<double> diff = end-start;
   std::cout << "Time for " << nTransfers << " transfers: " << diff.count() << " s\n";
   std::cout << "Average time per transfer: " << diff.count()/(double)nTransfers * 1e6 << " us\n";


   sender.join();

}

#endif

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(future_queue_when_any) {
    std::cout << "Measure performance of future_queue with when_any" << std::endl;

    static_assert(nTransfers % nQueues == 0, "nQueues must be an integer divider of nTransfers.");

    std::vector<future_queue<int32_t>> vectorOfQueues;
    for(size_t i=0; i<nQueues; ++i) vectorOfQueues.emplace_back(queueLength);

    auto notificationQueue = when_any(vectorOfQueues.begin(), vectorOfQueues.end());

    barrier b1(nQueues+1), b2(nQueues+1);

    std::vector<std::thread> senders;
    for(auto &q : vectorOfQueues) {
      senders.emplace_back( [&q, &b1, &b2] {
        b1.wait();
        b2.wait();
        for(size_t i=0; i<nTransfers/nQueues; ++i) {
          while(q.push(i & 0xFFFF) == false) usleep(1);
        }
      } );    // end thread sender
    }

    b1.wait();
    auto start = std::chrono::steady_clock::now();
    b2.wait();

    for(size_t i=0; i<nTransfers; ++i) {
      size_t id;
      notificationQueue.pop_wait(id);
      int32_t val;
      vectorOfQueues[id].pop(val);
    }

    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> diff = end-start;
    std::cout << "Time for " << nTransfers << " transfers: " << diff.count() << " s\n";
    std::cout << "Average time per transfer: " << diff.count()/(double)nTransfers * 1e6 << " us\n";

    for(auto &t : senders) t.join();

}

/*********************************************************************************************************************/

#ifdef ENABLE_BOOST_LOCKFREE_QUEUE_PERFORMANCE_MEASUREMENT

BOOST_AUTO_TEST_CASE(boost_spsc_queue_wait_for_any) {
    std::cout << "Measure performance of boost::lockfree::spsc_queue<boost::shared_future<T>> with wait_for_any" << std::endl;

    static_assert(nTransfers % nQueues == 0, "nQueues must be an integer divider of nTransfers.");

    std::list< std::unique_ptr< boost::lockfree::spsc_queue<boost::shared_future<int32_t>> > > listOfQueues;
    for(size_t i=0; i<nQueues; ++i) {
      listOfQueues.emplace_back(new boost::lockfree::spsc_queue<boost::shared_future<int32_t>>(queueLength));
    }

    barrier b1(nQueues+1), b2(nQueues+1), b3(nQueues+1);

    std::vector<std::thread> senders;
    for(auto &q : listOfQueues) {
      senders.emplace_back( [&q, &b1, &b2, &b3] {
        boost::promise<int32_t> thePromise;
        q->push(thePromise.get_future().share());
        b1.wait();
        b2.wait();
        for(size_t i=0; i<nTransfers/nQueues; ++i) {
          boost::promise<int32_t> newPromise;
          auto newFuture = newPromise.get_future().share();
          while(q->push(newFuture) == false) usleep(1);
          thePromise.set_value(i & 0xFFFF);
          thePromise = std::move(newPromise);
        }
        b3.wait();
      } );    // end thread sender
    }

    b1.wait();
    auto start = std::chrono::steady_clock::now();
    b2.wait();

    for(size_t i=0; i<nTransfers; ++i) {
      auto ret = boost::wait_for_any(helper_iterator(listOfQueues.begin()), helper_iterator(listOfQueues.end()));
      auto &theQueue = ret.get_queue();
      boost::shared_future<int32_t> theFuture;
      theQueue->pop(theFuture);
      theFuture.get();
   }

   auto end = std::chrono::steady_clock::now();
   std::chrono::duration<double> diff = end-start;
   std::cout << "Time for " << nTransfers << " transfers: " << diff.count() << " s\n";
   std::cout << "Average time per transfer: " << diff.count()/(double)nTransfers * 1e6 << " us\n";

   b3.wait();
   for(auto &t : senders) t.join();

}

#endif

/*********************************************************************************************************************/

BOOST_AUTO_TEST_SUITE_END()
