cmake_policy(SET CMP0048 NEW)

project(artemis VERSION 0.1.0
                LANGUAGES C CXX)

cmake_minimum_required(VERSION 3.2)

set(CMAKE_CXX_STANDARD 11)


list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
include(EnableMakeCheck)

find_package(EGrabber)
find_package(GLog REQUIRED)
find_package(OpenCV REQUIRED core imgcodecs highgui imgproc)
find_package(Protobuf 3.5.0 REQUIRED)
find_package(Asio 1.10.8 REQUIRED)
find_package(Eigen3 REQUIRED)

include(ExternalProject)
ExternalProject_Add(APRILTAG3
	GIT_REPOSITORY https://github.com/AprilRobotics/apriltags.git
	GIT_TAG v3.0.0
	PATCH_COMMAND sed -i.bak /@ldconfig/d Makefile && sed -i.bak s/-O3/-O0\ -g/ Makefile
	CONFIGURE_COMMAND echo done
	INSTALL_COMMAND PREFIX=${PROJECT_BINARY_DIR}/APRILTAG3-prefix make install
	BUILD_IN_SOURCE 1
	)
set(APRILTAG3_INCLUDE_DIRS ${PROJECT_BINARY_DIR}/APRILTAG3-prefix/include)
set(APRILTAG3_LIBRARIES ${PROJECT_BINARY_DIR}/APRILTAG3-prefix/lib/${CMAKE_SHARED_LIBRARY_PREFIX}apriltag${CMAKE_SHARED_LIBRARY_SUFFIX}
                        Threads::Threads
                        )
add_definitions("-D_GNU_SOURCE" "-DNDEBUG")

add_definitions(${EIGEN3_DEFINITIONS})

include_directories(${EGRABBER_INCLUDE_DIRS}
	                ${GLOG_INCLUDE_DIRS}
	                ${APRILTAG3_INCLUDE_DIRS}
	                ${OPENCV_INCLUDE_DIRS}
	                ${gflags_INCLUDE_DIRS}
	                ${EIGEN3_INCLUDE_DIRS}
	                )



find_package(GoogleMock REQUIRED)
enable_testing()
enable_make_check()


add_subdirectory(src)
