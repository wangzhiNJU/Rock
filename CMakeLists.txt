project(Rock CXX)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(SRC_LIST
    ab.cc
    event.cc
    ipc.cc
    tasktracker.cc
    proxyD.cc
    queues.cc
    poller.cc
    )
add_library(rock ${SRC_LIST})
