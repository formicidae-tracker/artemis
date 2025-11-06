cmake_policy(SET CMP0048 NEW)
cmake_policy(SET CMP0077 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

cmake_minimum_required(VERSION 3.16)

project(
	artemis
	VERSION 0.4.5
	LANGUAGES C CXX
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	add_definitions(-Wall -Wpedantic -Werror)
endif()

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED 1)

option(EURESYS_FRAMEGRABBER_SUPPORT
	   "Add support for Euresys Coaxpress Framegrabber" Off
)
option(HYPERION_FRAMEGRABBER_SUPPORT
	   "Add support for Hyperion Camera Link Framegrabber" Off
)
option(MULTICAM_FRAMEGRABBER_SUPPORT
	   "Add support for Multicam Camera Link Framegrabber" Off
)

set(ARTEMIS_FRAMEGRABBER)

if(EURESYS_FRAMEGRABBER_SUPPORT)
	find_package(EGrabber REQUIRED)
	list(APPEND ARTEMIS_FRAMEGRABBER "egrabber")
endif(EURESYS_FRAMEGRABBER_SUPPORT)

if(HYPERION_FRAMEGRABBER_SUPPORT)
	find_package(Hyperion REQUIRED)
	list(APPEND ARTEMIS_FRAMEGRABBER "hyperion")
endif(HYPERION_FRAMEGRABBER_SUPPORT)

if(MULTICAM_FRAMEGRABBER_SUPPORT)
	find_package(Multicam REQUIRED)
	list(APPEND ARTEMIS_FRAMEGRABBER "multicam")
endif(MULTICAM_FRAMEGRABBER_SUPPORT)

set(ARTEMIS_STUB_FRAMEGRABBER_ONLY Off)
list(LENGTH ARTEMIS_FRAMEGRABBER ARTEMIS_NUM_FG)
if(ARTEMIS_NUM_FG EQUAL 0)
	set(ARTEMIS_STUB_FRAMEGRABBER_ONLY On)
elseif(ARTEMIS_NUM_FG GREATER 1)
	message(
		FATAL_ERROR
			"Artemis only support one framegrabber at a time, currently selected: ${ARTEMIS_FRAMEGRABBER}"
	)
endif(ARTEMIS_NUM_FG EQUAL 0)

include(FetchContent)

set(FORT_CHARIS_BUILD_GL On)
set(FORT_CHARIS_BUILD_VIDEO Off)
set(FORT_CHARIS_CAPITALIZE_DEFER On)

set(CPPTRACE_UNWIND_WITH_LIBUNWIND On)

FetchContent_Declare(
	slogpp
	GIT_REPOSITORY https://github.com/atuleu/slogpp.git
	GIT_TAG 26ca0aa97b726fcaa4a4e4b0bfa312a0267a387b
)

if(EXISTS ${PROJECT_SOURCE_DIR}/deps/charis)
	FetchContent_Declare(
		fort-charis SOURCE_DIR ${PROJECT_SOURCE_DIR}/deps/charis
	)
else()
	FetchContent_Declare(
		fort-charis
		GIT_REPOSITORY https://github.com/formicidae-tracker/charis.git
		GIT_TAG 3df2c8929118298e8776e0ef3ca0c05b700dc43a
	)
endif()

if(EXISTS ${PROJECT_SOURCE_DIR}/deps/charis)
	FetchContent_Declare(
		fort-hermes SOURCE_DIR ${PROJECT_SOURCE_DIR}/deps/hermes
	)
else()
	FetchContent_Declare(
		fort-hermes
		GIT_REPOSITORY https://github.com/formicidae-tracker/hermes.git
		GIT_TAG v0.5.7
	)
endif()

if(EXISTS ${PROJECT_SOURCE_DIR}/deps/fort-tags)
	FetchContent_Declare(
		fort-tags SOURCE_DIR ${PROJECT_SOURCE_DIR}/deps/fort-tags
	)
else()
	FetchContent_Declare(
		fort-tags
		GIT_REPOSITORY https://github.com/formicidae-tracker/fort-tags.git
		GIT_TAG v1.4.3
	)
endif()
FetchContent_Declare(
	taskflow
	GIT_REPOSITORY https://github.com/taskflow/taskflow
	GIT_TAG v3.10.0
)

FetchContent_GetProperties(taskflow)
if(NOT taskflow_POPULATED)
	FetchContent_Populate(taskflow)
endif(NOT taskflow_POPULATED)

add_library(taskflow INTERFACE)
add_library(taskflow::taskflow ALIAS taskflow)
target_include_directories(taskflow INTERFACE ${taskflow_SOURCE_DIR})
target_link_options(taskflow INTERFACE "-pthread")

FetchContent_Declare(
	readwriterqueue
	GIT_REPOSITORY https://github.com/cameron314/readerwriterqueue
	GIT_TAG v1.0.7
)

FetchContent_Declare(
	libyuv
	GIT_REPOSITORY https://chromium.googlesource.com/libyuv/libyuv
	GIT_TAG 900da61d3cadba86ec593c8226de736b5e6b2c43
)

FetchContent_Declare(
	spng
	GIT_REPOSITORY https://github.com/randy408/libspng.git
	GIT_TAG v0.7.4
)

FetchContent_MakeAvailable(
	fort-charis
	fort-hermes
	fort-tags
	readwriterqueue
	libyuv
	spng
	slogpp
)
FetchContent_GetProperties(fort-charis)
FetchContent_GetProperties(libyuv)

add_library(libyuv::libyuv ALIAS yuv)
target_include_directories(yuv INTERFACE ${libyuv_SOURCE_DIR}/include)

find_package(Eigen3 REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(Protobuf 3.3.0 REQUIRED)

add_definitions(${EIGEN3_DEFINITIONS})

include_directories(
	${EGRABBER_INCLUDE_DIRS} ${EIGEN3_INCLUDE_DIRS} ${PROTOBUF_INCLUDE_DIRS}
)

set(INSTALL_GTEST Off)
FetchContent_Declare(
	googletest
	GIT_REPOSITORY https://github.com/google/googletest.git
	GIT_TAG 76bb2afb8b522d24496ad1c757a49784fbfa2e42
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt
	ON
	CACHE BOOL "" FORCE
)
FetchContent_MakeAvailable(googletest)

enable_testing()
add_custom_target(check ${CMAKE_CTEST_COMMAND} ARGS --output-on-failure)
include(GoogleTest)

# add_subdirectory(scripts)

add_subdirectory(src)
