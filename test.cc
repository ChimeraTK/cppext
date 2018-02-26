#include "future_queue.h"

int main() {

    // create two queues
    size_t nbuffers = 3;
    future_queue<int> a(nbuffers), b(nbuffers);
/*    
    a.push(3);
    std::cout << "r " << a.read_available() << "  w " << a.write_available() << std::endl;
    assert(a.read_available() == 1);
    assert(a.write_available() == nbuffers-1);
    for(int i=0; i<100; ++i) {
      std::cout << "push" << std::endl;
      a.push(3);
      std::cout << "r " << a.read_available() << "  w " << a.write_available() << std::endl;
      assert(a.read_available() == 2);
      assert(a.write_available() == nbuffers-2);
      int x;
      std::cout << "pop" << std::endl;
      bool t = a.pop(x);
      assert(t);
      std::cout << "r " << a.read_available() << "  w " << a.write_available() << std::endl;
      assert(a.read_available() == 1);
      assert(a.write_available() == nbuffers-1);
    }
    
    return 0;
    */
    
    // flag for termiation of the threads
    std::atomic<bool> terminate;
    terminate = false;

    // create sender thread 1
    std::thread( [&a, &terminate] {
      
      int counter = 0;
      while(!terminate) {
        // increment counter and push its value to the queue - potentially overwriting the last value if the queue is
        // full
        int val = ++counter;
//        a.push_overwrite( std::move(val) );
        a.push( std::move(val) );
      }
      
    }).detach();

    // create sender thread 2
    std::thread( [&b, &terminate] {
      
      int counter = 0;
      while(!terminate) {
        // increment counter and push its value to the queue - potentially overwriting the last value if the queue is
        // full
        int val = ++counter;
//        b.push_overwrite( std::move(val) );
        b.push( std::move(val) );
      }
      
    }).detach();

    usleep(1000000);

    //for(int i=0; i<5000; ++i) {
    while(true) {
      int k;
      // wait until any of the two queues has new data
      wait_any(a, b);
      std::cout << "any " << std::endl;
      // we don't know which one has the data, so check them both
      bool hadData = false;
      if(a.has_data()) {
        // pop element from queue and print it
        //a.pop_wait(k);
        bool t = a.pop(k);
        assert(t);  // read_available() told us so
        std::cout << "A " << k << std::endl;
        hadData = true;
      }
      if(b.has_data()) {
        // pop element from queue and print it
        //b.pop_wait(k);
        bool t = b.pop(k);
        assert(t);  // read_available() told us so
        std::cout << "B " << k << std::endl;
        hadData = true;
      }
      assert(hadData);        // no guarantee!
    }
    terminate = true;
    
    return 0;

}
