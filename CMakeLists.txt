cmake_policy(SET CMP0048 NEW)

project(artemis VERSION 0.3.1
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
                     GIT_TAG        v1.2.2)

FetchContent_Declare(hermes
                     GIT_REPOSITORY https://github.com/formicidae-tracker/hermes.git
                     GIT_TAG        v0.2.3)



FetchContent_GetProperties(googletest)
if(NOT googletest_POPULATED)
	FetchContent_Populate(googletest)
	add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR} EXCLUDE_FROM_ALL)
endif(NOT googletest_POPULATED)

option(EURESYS_FRAMEGRABBER_SUPPORT "Add support for Euresys Framegrabber" On)
option(FORCE_STUB_FRAMEGRABBER_ONLY "Removes support for any other framegraber than stub" Off)

if(FORCE_STUB_FRAMEGRABBER_ONLY)
	set(EURESYS_FRAMEGRABBER_SUPPORT Off)
endif(FORCE_STUB_FRAMEGRABBER_ONLY)


FetchContent_GetProperties(fort-tags)
if(NOT fort-tags_POPULATED)
	FetchContent_Populate(fort-tags)
	add_subdirectory(${fort-tags_SOURCE_DIR} ${fort-tags_BINARY_DIR})
endif(NOT fort-tags_POPULATED)

FetchContent_GetProperties(hermes)
if(NOT hermes_POPULATED)
	FetchContent_Populate(hermes)
	add_subdirectory(${hermes_SOURCE_DIR} ${hermes_BINARY_DIR})
endif(NOT hermes_POPULATED)
set(HERMES_INCLUDE_DIRS ${hermes_SOURCE_DIR}/src ${hermes_BINARY_DIR}/src)

if(EURESYS_FRAMEGRABBER_SUPPORT)
	find_package(EGrabber REQUIRED)
endif(EURESYS_FRAMEGRABBER_SUPPORT)
find_package(GLog REQUIRED)
find_package(OpenCV REQUIRED core highgui imgproc imgcodecs)
find_package(Eigen3 REQUIRED)
find_package(Asio)


find_package(Protobuf 3.3.0 REQUIRED)


add_definitions("-D_GNU_SOURCE" "-DNDEBUG")

add_definitions(${EIGEN3_DEFINITIONS})
include_directories(${EGRABBER_INCLUDE_DIRS}
                    ${ASIO_INCLUDE_DIRS}
	                ${GLOG_INCLUDE_DIRS}
	                ${OpenCV_INCLUDE_DIRS}
	                ${gflags_INCLUDE_DIRS}
	                ${EIGEN3_INCLUDE_DIRS}
	                ${HERMES_INCLUDE_DIRS}
	                ${APRILTAG_INCLUDE_DIRS}
	                ${fort-tags_SOURCE_DIR}/include
	                ${googletest_SOURCE_DIR}/googlemock/include
	                ${googletest_SOURCE_DIR}/googletest/include
	                )


include(GoogleTest)
include(EnableMakeCheck)
enable_testing()
enable_make_check()


add_subdirectory(src)


install(FILES share/vga.fon DESTINATION share/artemis/)
