#include <mutex>

// define thread-safe variants of BOOST_CHECK etc.
std::mutex boostCheckMutex;

#define BOOST_CHECK_TS(condition)                                                                                      \
  if(!(condition)) {                                                                                                   \
    std::unique_lock<std::mutex> lock(boostCheckMutex);                                                                \
    BOOST_CHECK(condition);                                                                                            \
  }

#define BOOST_CHECK_EQUAL_TS(a, b)                                                                                     \
  if((a) != (b)) {                                                                                                     \
    std::unique_lock<std::mutex> lock(boostCheckMutex);                                                                \
    BOOST_CHECK_EQUAL(a, b);                                                                                           \
  }

#define BOOST_ERROR_TS(message)                                                                                        \
  {                                                                                                                    \
    std::unique_lock<std::mutex> lock(boostCheckMutex);                                                                \
    BOOST_ERROR(message);                                                                                              \
  }

#define BOOST_CHECK_TIMEOUT(condition)                                                                                 \
  {                                                                                                                    \
    bool isOk = false;                                                                                                 \
    for(size_t i = 0; i < 1000; ++i) {                                                                                 \
      if(condition) {                                                                                                  \
        isOk = true;                                                                                                   \
        break;                                                                                                         \
      }                                                                                                                \
      usleep(10000);                                                                                                   \
    }                                                                                                                  \
    if(!isOk) BOOST_ERROR_TS("Check with timeout on condition failed: " #condition);                                   \
  }
