cmake_policy(SET CMP0048 NEW)

project(artemis VERSION 0.2.3
                LANGUAGES C CXX)

cmake_minimum_required(VERSION 3.11)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

set(CMAKE_CXX_STANDARD 11)

include(FetchContent)
FetchContent_Declare(googletest
                     GIT_REPOSITORY https://github.com/google/googletest.git
                     GIT_TAG        release-1.8.1
                     )

FetchContent_Declare(fort-tags
                     GIT_REPOSITORY https://github.com/formicidae-tracker/fort-tags.git
                     GIT_TAG        v1.0.0)

FetchContent_GetProperties(googletest)
if(NOT googletest_POPULATED)
	FetchContent_Populate(googletest)
	add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR} EXCLUDE_FROM_ALL)
endif(NOT googletest_POPULATED)


FetchContent_GetProperties(fort-tags)
if(NOT apriltag_POPULATED)
	FetchContent_Populate(fort-tags)
	add_subdirectory(${fort-tags_SOURCE_DIR} ${fort-tags_BINARY_DIR})
endif(NOT apriltag_POPULATED)



find_package(EGrabber REQUIRED)
find_package(GLog REQUIRED)
find_package(OpenCV REQUIRED core highgui imgproc imgcodecs)
find_package(Eigen3 REQUIRED)
find_package(Asio)


find_package(FortHermesCpp 0.2.0 REQUIRED)
find_package(Protobuf 3.3.0 REQUIRED)


add_definitions("-D_GNU_SOURCE" "-DNDEBUG")

add_definitions(${EIGEN3_DEFINITIONS})

include_directories(${EGRABBER_INCLUDE_DIRS}
                    ${ASIO_INCLUDE_DIRS}
	                ${GLOG_INCLUDE_DIRS}
	                ${OPENCV_INCLUDE_DIRS}
	                ${gflags_INCLUDE_DIRS}
	                ${EIGEN3_INCLUDE_DIRS}
	                ${FORT_HERMES_CPP_INCLUDE_DIRS}
	                ${fort-tags_SOURCE_DIR}/src
	                )


include(GoogleTest)
include(EnableMakeCheck)
enable_testing()
enable_make_check()


add_subdirectory(src)


install(FILES share/vga.fon DESTINATION share/artemis/)
