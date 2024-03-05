cmake_minimum_required(VERSION 3.5)

project(keyvalues)

add_library(keyvalues STATIC ${CMAKE_CURRENT_LIST_DIR}/KeyValue.cpp ${CMAKE_CURRENT_LIST_DIR}/KeyValue.h)
target_include_directories(keyvalues PUBLIC ${CMAKE_CURRENT_LIST_DIR})
