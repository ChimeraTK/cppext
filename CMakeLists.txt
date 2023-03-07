project(ChimeraTK-cppext)
cmake_minimum_required(VERSION 2.8)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/Modules)

set(${PROJECT_NAME}_MAJOR_VERSION 01)
set(${PROJECT_NAME}_MINOR_VERSION 04)
set(${PROJECT_NAME}_PATCH_VERSION 02)
include(cmake/set_version_numbers.cmake)

# find BOOST libraries (needed for tests only)
find_package(Boost COMPONENTS unit_test_framework system thread REQUIRED)
find_package(Threads REQUIRED)
find_package(GccAtomic REQUIRED)

# libatomic does not exist on all systems (e.g. when using Clang / LLVM), so
# we ignore if it is missing.
IF(NOT GCCLIBATOMIC_FOUND)
  set(GCCLIBATOMIC_LIBRARY "")
ENDIF()

# Set default compiler flags and build types
include(cmake/set_default_flags.cmake)

# add support for latest C++ standard to compiler flags
include(cmake/enable_latest_cxx_support.cmake)

include_directories(${CMAKE_SOURCE_DIR}/include)

option(BUILD_TESTS "Build test programs" ON)
if(BUILD_TESTS)
  enable_testing()

  aux_source_directory(${CMAKE_SOURCE_DIR}/tests testExecutables)
  foreach( testExecutableSrcFile ${testExecutables})
    #NAME_WE means the base name without path and (longest) extension
    get_filename_component(excutableName ${testExecutableSrcFile} NAME_WE)
    add_executable(${excutableName} ${testExecutableSrcFile})
    target_link_libraries(${excutableName} ${Boost_THREAD_LIBRARY} ${Boost_SYSTEM_LIBRARY} ${CMAKE_THREAD_LIBS_INIT} ${GCCLIBATOMIC_LIBRARY})
    add_test(${excutableName} ${excutableName})
  endforeach( testExecutableSrcFile )

endif()

# generate Doxygen documentation
include(cmake/enable_doxygen_documentation.cmake)

# enable code coverate report
include(cmake/enable_code_coverage_report.cmake)

# install header files
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION include/ChimeraTK/cppext
                                                  FILES_MATCHING PATTERN "*.hpp")

# Create cmake configuration files for downstream projects
set(${PROJECT_NAME}_INCLUDE_DIRS "${CMAKE_INSTALL_PREFIX}/include")
set(${PROJECT_NAME}_LIBRARIES "${GCCLIBATOMIC_LIBRARY}")
set(${PROJECT_NAME}_LIBRARY_DIRS "")
# already set: ${PROJECT_NAME}_CXX_FLAGS
include(${CMAKE_SOURCE_DIR}/cmake/create_cmake_config_files.cmake)
