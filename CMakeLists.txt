project(Rock CXX)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(SRC_LIST
    ab.cc
    )
add_library(rock ${SRC_LIST})
