cmake_policy(SET CMP0048 NEW)

project(artemis VERSION 0.1.0
                LANGUAGES C CXX)

cmake_minimum_required(VERSION 3.2)

set(CMAKE_CXX_STANDARD 11)


list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
include(EnableMakeCheck)

find_package(GoogleMock REQUIRED)
enable_testing()
enable_make_check()


add_subdirectory(src)
