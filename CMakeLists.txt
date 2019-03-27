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


include(ExternalProject)
ExternalProject_Add(APRILTAG2
	URL https://april.eecs.umich.edu/media/apriltag/apriltag-2016-12-01.tgz
	URL_HASH MD5=5607f8c4eba3dfa0c62bcd77b751aef3
	PATCH_COMMAND sed -i.bak /@ldconfig/d Makefile
	CONFIGURE_COMMAND echo done
	INSTALL_COMMAND PREFIX=${PROJECT_BINARY_DIR}/APRILTAG2-prefix make install
	BUILD_IN_SOURCE 1
	)
set(APRILTAG2_INCLUDE_DIRS ${PROJECT_BINARY_DIR}/APRILTAG2-prefix/include)
set(APRILTAG2_LIBRARIES ${PROJECT_BINARY_DIR}/APRILTAG2-prefix/lib/${CMAKE_SHARED_LIBRARY_PREFIX}apriltag${CMAKE_SHARED_LIBRARY_SUFFIX}
                        Threads::Threads
                        )
add_definitions("-D_GNU_SOURCE")



include_directories(${EGRABBER_INCLUDE_DIRS}
	                ${GLOG_INCLUDE_DIRS}
	                ${APRILTAG2_INCLUDE_DIRS}
	                ${OPENCV_INCLUDE_DIRS}
	                ${gflags_INCLUDE_DIRS}
	                )



find_package(GoogleMock REQUIRED)
enable_testing()
enable_make_check()


add_subdirectory(src)
