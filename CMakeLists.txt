cmake_minimum_required (VERSION 2.8.11)
project (infinite-otp)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11 -O3")

add_executable (infinite-otp Infinite-OTPV2.cpp)

target_link_libraries (infinite-otp LINK_PUBLIC ssl)
