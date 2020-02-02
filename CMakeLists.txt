cmake_minimum_required (VERSION 3.10.2)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

find_package (Threads)

find_package(Boost 1.65.1 REQUIRED COMPONENTS system thread regex)
if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
  link_directories(${Boost_LIBRARY_DIRS})
endif(Boost_FOUND)

set(USED_LIBS ${Boost_SYSTEM_LIBRARY} ${Boost_THREAD_LIBRARY} ${Boost_REGEX_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})

add_executable (client client.cpp)
target_link_libraries(client ${USED_LIBS})
add_executable (server server.cpp)
target_link_libraries(server ${USED_LIBS})
