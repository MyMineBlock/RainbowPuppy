cmake_minimum_required(VERSION 4.0.2)
project(RainbowPuppy LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(src)
add_subdirectory(vendor/ViGEm)
add_subdirectory(vendor/interception)