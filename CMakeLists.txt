PROJECT(future_queue_tests)
cmake_minimum_required(VERSION 2.8)

# find BOOST unit test framework
FIND_PACKAGE(Boost COMPONENTS unit_test_framework thread REQUIRED)


# Moderate version of the compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -fPIC -Wextra -Wuninitialized")

# use -DCMAKE_BUILD_TYPE=Debug in your cmake command to turn on the coverage option
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 --coverage -fno-inline -fno-inline-small-functions -fno-default-inline")


include_directories(${CMAKE_SOURCE_DIR})

ENABLE_TESTING()

aux_source_directory(${CMAKE_SOURCE_DIR}/src testExecutables)
foreach( testExecutableSrcFile ${testExecutables})
  #NAME_WE means the base name without path and (longest) extension
  get_filename_component(excutableName ${testExecutableSrcFile} NAME_WE)
  add_executable(${excutableName} ${testExecutableSrcFile})
  target_link_libraries(${excutableName} ${Boost_LIBRARIES})
  add_test(${excutableName} ${excutableName})
endforeach( testExecutableSrcFile )

# enable code coverate report
include(cmake/enable_code_coverage_report.cmake)


