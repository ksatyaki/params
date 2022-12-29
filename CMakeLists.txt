cmake_minimum_required(VERSION 2.8.11)
project(params)

set(CMAKE_CXX_STANDARD 11)

add_library(params INTERFACE)
target_include_directories(params INTERFACE include/)
