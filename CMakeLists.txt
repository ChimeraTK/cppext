project(ChimeraTK-cppext)
cmake_minimum_required(VERSION 3.15)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/Modules)
find_package(GccAtomic)

file(GLOB headers include/*.hpp)
add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
if (GLIBCCATOMIC_FOUND)
    target_link_libraries(${PROJECT_NAME} INTERFACE
            $<BUILD_INTERFACE:${GCCLIBATOMIC_LIBRARY}>
            $<INSTALL_INTERFACE:atomic>)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES PUBLIC_HEADER "${headers}")

install(TARGETS ${PROJECT_NAME}
        DESTINATION include/ChimeraTK/cppext
        EXPORT ${PROJECT_NAME}Targets)

install(EXPORT ${PROJECT_NAME}Targets
        FILE ${PROJECT_NAME}Targets.cmake
        DESTINATION lib/cmake/${PROJECT_NAME})

option(BUILD_TESTS "Build test programs" ON)
if (BUILD_TESTS)
    find_package(Boost COMPONENTS unit_test_framework system thread REQUIRED)
    find_package(Threads REQUIRED)
    enable_testing()

    file(GLOB testSources "tests/*.cc")
    foreach(testExecutableSrcFile IN LISTS testSources)
        get_filename_component(executableName ${testExecutableSrcFile} NAME_WE)
        add_executable(${executableName} ${testExecutableSrcFile})
        target_link_libraries(${executableName} PUBLIC ${PROJECT_NAME} Boost::thread Threads::Threads ${GCCLIBATOMIC_LIBRARY})
        add_test(${executableName} ${executableName})
    endforeach()
endif()
