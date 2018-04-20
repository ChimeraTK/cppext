#define BOOST_TEST_MODULE test_future_queue
#include <boost/test/included/unit_test.hpp>
using namespace boost::unit_test_framework;

#include <random>

#include <boost/thread/thread.hpp>

#include "future_queue.hpp"
#include "barrier.hpp"

BOOST_AUTO_TEST_SUITE(testObtainWriteSlot)

/*********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testObtainWriteSlot) {

    constexpr size_t runIterations = 1000;
    constexpr size_t nThreads = 50;
    constexpr size_t lengthOfQueue = 10;

    // create the queue
    future_queue<int> q(lengthOfQueue);
    BOOST_CHECK_EQUAL( q.d->nBuffers, lengthOfQueue+1 );

    // list of threads so we can collect them later
    std::list<boost::thread> threads;

    // barriers for test synchronisation. "+1" is for the main thread
    barrier b0(nThreads+1), b1(nThreads+1), b2(nThreads+1), b3(nThreads+1);

    // result of each test iteration
    std::mutex resultMutex;
    std::map<size_t, size_t> slotCounter;
    size_t noSlotCounter = 0;

    // launch threads
    for(size_t i=0; i<nThreads; ++i) {
      threads.emplace_back( [&q, &b0, &b1, &b2, &b3, &resultMutex, &slotCounter, &noSlotCounter] () mutable {
        while(true) {
          b0.wait();
          boost::this_thread::interruption_point();

          size_t slot;
          bool hasSlot = q.obtain_write_slot(slot);
          slot = slot % (lengthOfQueue+1);

          // wait until all threads reached this point
          b1.wait();

          // store result
          {
            std::unique_lock<std::mutex> lock(resultMutex);
            if(hasSlot) {
              slotCounter[slot]++;
            }
            else {
              noSlotCounter++;
            }
          }

          // notify that we are ready storing the result
          b2.wait();

          // wait until main thread allows us to continue
          b3.wait();

        }
      } );  // end sender thread
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, lengthOfQueue);

    for(size_t iteration = 0; iteration < runIterations; ++iteration) {

      // reset queue for next test
      size_t readIndex = iteration;
      size_t nElemsInQueue = dis(gen);
      q.d->readIndex = readIndex;
      size_t writeIndex = readIndex+nElemsInQueue;
      q.d->writeIndex = writeIndex;

      //std::cout << "iteration = " << iteration << " q.readIndex = " << q.readIndex << " q.writeIndex = " << q.writeIndex << std::endl;

      // reset test result
      for(size_t i=0; i < lengthOfQueue+1; ++i) {
        slotCounter[i] = 0;
      }
      noSlotCounter = 0;

      // let all threads obtain a slot and wait until they are done
      b0.wait();
      b1.wait();

      // check result
      b2.wait();
      BOOST_CHECK_EQUAL( q.d->readIndex, readIndex );
      BOOST_CHECK_EQUAL( q.d->writeIndex, readIndex+lengthOfQueue );
      BOOST_CHECK_EQUAL( noSlotCounter, nThreads - lengthOfQueue + nElemsInQueue );

      // these slots must not be used
      for(size_t i=readIndex; i < readIndex+nElemsInQueue; ++i) {
        BOOST_CHECK_EQUAL( slotCounter[i % (lengthOfQueue+1)], 0 );
      }
      // these slots must be used
      for(size_t i=writeIndex; i < writeIndex+lengthOfQueue-nElemsInQueue; ++i) {
        BOOST_CHECK_EQUAL( slotCounter[i % (lengthOfQueue+1)], 1 );
      }

      // allow all theads to continue
      b3.wait();
    }

    for(auto &t : threads) t.interrupt();
    b0.wait();
    for(auto &t : threads) t.join();

}

/*********************************************************************************************************************/

BOOST_AUTO_TEST_SUITE_END()
