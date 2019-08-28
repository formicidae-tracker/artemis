cmake_policy(SET CMP0048 NEW)

project(artemis VERSION 0.1.5
                LANGUAGES C CXX)

cmake_minimum_required(VERSION 3.2)

set(CMAKE_CXX_STANDARD 11)


list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
include(EnableMakeCheck)

find_package(EGrabber REQUIRED)
find_package(GLog REQUIRED)
find_package(OpenCV REQUIRED core highgui imgproc imgcodecs)
find_package(FortHermesCpp 0.1.11 REQUIRED)
find_package(Protobuf 3.3.0 REQUIRED)
find_package(Eigen3 REQUIRED)

find_package(Asio EXACT 1.10.8)
if(NOT Asio_FOUND)
	include(ExternalProject)
	include(ExternalProject)
	ExternalProject_Add(ASIO
		URL https://sourceforge.net/projects/asio/files/asio/1.10.8%20%28Stable%29/asio-1.10.8.tar.bz2/download
		BUILD_IN_SOURCE 1
		CONFIGURE_COMMAND ./configure --prefix=${PROJECT_BINARY_DIR}/ASIO-prefix
		BUILD_COMMAND echo "done"
		TEST_BEFORE_INSTALL 1
		TEST_COMMAND make check -j 7
		INSTALL_COMMAND make install)
	set(ASIO_INCLUDE_DIRS ${PROJECT_BINARY_DIR}/ASIO-prefix/include)
endif(NOT Asio_FOUND)



find_package(PkgConfig REQUIRED)
pkg_check_modules(APRILTAG3 apriltag>=3.0.0)

add_definitions("-D_GNU_SOURCE" "-DNDEBUG")

add_definitions(${EIGEN3_DEFINITIONS})

include_directories(${EGRABBER_INCLUDE_DIRS}
                    ${ASIO_INCLUDE_DIRS}
	                ${GLOG_INCLUDE_DIRS}
	                ${APRILTAG3_INCLUDE_DIRS}
	                ${OPENCV_INCLUDE_DIRS}
	                ${gflags_INCLUDE_DIRS}
	                ${EIGEN3_INCLUDE_DIRS}
	                ${FORT_HERMES_CPP_INCLUDE_DIRS}
	                )



find_package(GoogleMock REQUIRED)
enable_testing()
enable_make_check()


add_subdirectory(src)


install(FILES share/vga.fon DESTINATION share/artemis/)
