project(future_queue_tests)
cmake_minimum_required(VERSION 2.8)

# find BOOST libraries
find_package(Boost COMPONENTS unit_test_framework thread REQUIRED)
find_package (Threads REQUIRED)

# Moderate version of the compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -fPIC -Wextra -Wuninitialized")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -fno-inline -fno-inline-small-functions -fno-default-inline")

include_directories(${CMAKE_SOURCE_DIR})

enable_testing()

aux_source_directory(${CMAKE_SOURCE_DIR}/src testExecutables)
foreach( testExecutableSrcFile ${testExecutables})
  #NAME_WE means the base name without path and (longest) extension
  get_filename_component(excutableName ${testExecutableSrcFile} NAME_WE)
  add_executable(${excutableName} ${testExecutableSrcFile})
  target_link_libraries(${excutableName} ${Boost_THREAD_LIBRARY} ${Boost_SYSTEM_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})
  add_test(${excutableName} ${excutableName})
endforeach( testExecutableSrcFile )

# enable code coverate report
include(cmake/enable_code_coverage_report.cmake)


