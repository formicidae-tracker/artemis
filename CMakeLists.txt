cmake_policy(SET CMP0048 NEW)
cmake_policy(SET CMP0077 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

project(artemis VERSION 0.4.5
	LANGUAGES C CXX)

cmake_minimum_required(VERSION 3.16)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

include(FetchGoogleTest)
include(FetchFortTags)
include(FetchFortHermes)
include(FetchBoostAsio)
include(FetchFreetypeGL)
include(FetchFortTime)

set(CMAKE_CXX_STANDARD 17)


option(EURESYS_FRAMEGRABBER_SUPPORT "Add support for Euresys Framegrabber" On)
option(FORCE_STUB_FRAMEGRABBER_ONLY "Removes support for any other framegraber than stub" Off)

if(FORCE_STUB_FRAMEGRABBER_ONLY)
	set(EURESYS_FRAMEGRABBER_SUPPORT Off)
endif(FORCE_STUB_FRAMEGRABBER_ONLY)

if(EURESYS_FRAMEGRABBER_SUPPORT)
	find_package(EGrabber REQUIRED)
endif(EURESYS_FRAMEGRABBER_SUPPORT)

fetch_google_test(VERSION release-1.12.1)
fetch_fort_tags(VERSION v1.4.1)
fetch_fort_time(VERSION v1.0.9)
fetch_fort_hermes(VERSION v0.4.1)
fetch_boost_asio(VERSION boost-1.74.0)
fetch_freetype_gl(VERSION 513fa238357f7c11a9f427965fe336aae7955297)


find_package(GLog REQUIRED)
find_package(OpenCV REQUIRED core highgui imgproc imgcodecs)
find_package(Eigen3 REQUIRED)
find_package(PkgConfig REQUIRED)
set(OpenGL_PREFERENCE GLVND)
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(GLEW REQUIRED)
find_package(Protobuf 3.3.0 REQUIRED)

pkg_check_modules(TBB tbb REQUIRED)

pkg_check_modules(FONTCONFIG fontconfig freetype2 REQUIRED)


add_definitions(${EIGEN3_DEFINITIONS})

include_directories(${EGRABBER_INCLUDE_DIRS}
                    ${GLOG_INCLUDE_DIRS}
                    ${OpenCV_INCLUDE_DIRS}
                    ${EIGEN3_INCLUDE_DIRS}
                    ${OPENGL_INCLUDE_DIRS}
                    # GLFW uses target properties
                    ${GLEW_INCLUDE_DIRS}
                    ${PROTOBUF_INCLUDE_DIRS}
                    ${TBB_INCLUDE_DIRS}
                    ${FONTCONFIG_INCLUDE_DIRS}
                    ${FORT_TIME_INCLUDE_DIRS}
                    ${FORT_TAGS_INCLUDE_DIRS}
                    ${FORT_HERMES_INCLUDE_DIRS}
                    ${BOOST_ASIO_INCLUDE_DIRS}
                    ${FREETYPE_GL_INCLUDE_DIRS}
)

include(EnableMakeCheck)
enable_testing()
enable_make_check()

add_subdirectory(scripts)

add_subdirectory(src)
