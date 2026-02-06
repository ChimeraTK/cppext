cmake_minimum_required(VERSION 3.16)
project(ChimeraTK-cppext)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/Modules)

set(${PROJECT_NAME}_MAJOR_VERSION 01)
set(${PROJECT_NAME}_MINOR_VERSION 07)
set(${PROJECT_NAME}_PATCH_VERSION 01)
include(cmake/set_version_numbers.cmake)

# our lib is header-only
add_library(${PROJECT_NAME} INTERFACE)

# find BOOST libraries (needed for tests only)
find_package(Boost COMPONENTS unit_test_framework system thread REQUIRED)
find_package(Threads REQUIRED)
find_package(GccAtomic)

# libatomic does not exist on all systems (e.g. when using Clang / LLVM), so
# we ignore if it is missing.
IF(GccAtomic_FOUND)
  target_link_libraries(${PROJECT_NAME} INTERFACE ${GccAtomic_LIBRARY})
ENDIF()

# Set default compiler flags and build types
include(cmake/set_default_flags.cmake)
include(cmake/enable_code_style_check.cmake)

# Require C++23 standard. This will be propagated to all targets that link against this library.
target_compile_features(${PROJECT_NAME} INTERFACE cxx_std_23)

target_include_directories(${PROJECT_NAME} INTERFACE
  "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>"

  # relative to ${CMAKE_INSTALL_PREFIX} but don't explicitly mention it
  "$<INSTALL_INTERFACE:include>")

option(BUILD_TESTS "Build test programs" ON)

if(BUILD_TESTS)
  enable_testing()

  aux_source_directory(${CMAKE_SOURCE_DIR}/tests testExecutables)

  foreach(testExecutableSrcFile ${testExecutables})
    # NAME_WE means the base name without path and (longest) extension
    get_filename_component(excutableName ${testExecutableSrcFile} NAME_WE)
    add_executable(${excutableName} ${testExecutableSrcFile})
    target_link_libraries(${excutableName} PRIVATE ${PROJECT_NAME} ${Boost_THREAD_LIBRARY} ${Boost_SYSTEM_LIBRARY} Threads::Threads)
    add_test(${excutableName} ${excutableName})
  endforeach(testExecutableSrcFile)
endif()

# generate Doxygen documentation
include(cmake/enable_doxygen_documentation.cmake)

# enable code coverate report
include(cmake/enable_code_coverage_report.cmake)

# install header files
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION include/ChimeraTK/cppext
  FILES_MATCHING PATTERN "*.hpp")

# this only provides cmake export
include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME} EXPORT ${PROJECT_NAME}Targets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

# Create cmake configuration files for downstream projects
# we support our cmake EXPORTS as imported targets
set(PROVIDES_EXPORTED_TARGETS 1)

# we need the public dependencies so create_cmake_config_files can find them as implicit dependencies
# Note, GccAtomic is not listed here since it doesn't provide imported target and resolved lib location
# is already in our public linker flags.
list(APPEND ${PROJECT_NAME}_PUBLIC_DEPENDENCIES "Boost COMPONENTS system thread;Threads")
include(${CMAKE_SOURCE_DIR}/cmake/create_cmake_config_files.cmake)
