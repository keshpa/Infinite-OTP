cmake_minimum_required (VERSION 2.8.11)
project (infinite-otp)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

add_executable (infinite-otp Infinite-OTP.cpp)

target_link_libraries (infinite-otp LINK_PUBLIC ssl)
