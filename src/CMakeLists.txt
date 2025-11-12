include(${fort-charis_SOURCE_DIR}/cmake/add_resources.cmake)

include(CheckCXXSourceRuns)

check_cxx_source_runs(
	"
#include <system_error>


int main() {
	if (std::error_code(EDOM,std::system_category()) == std::errc::argument_out_of_domain ) {
		return 0;
	}
	return 1;
}
"
	ARTEMIS_USE_SYSTEM_CATEGORY
)

configure_file(config.h.in artemis-config.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

set(RESOURCE_FILES
	ui/shaders/frame.vertexshader
	ui/shaders/frame.fragmentshader
	ui/shaders/primitive.vertexshader
	ui/shaders/primitive.fragmentshader
	ui/shaders/circle.fragmentshader
	ui/shaders/roi.fragmentshader
	fonts/vga.fon
)

set(SRC_FILES
	utils/PosixCall.cpp
	utils/StringManipulation.cpp
	utils/Partitions.cpp
	ImageU8.cpp
	Task.cpp
	Options.cpp
	FrameGrabber.cpp
	StubFrameGrabber.cpp
	Connection.cpp
	Application.cpp
	AcquisitionTask.cpp
	ProcessFrameTask.cpp
	ApriltagDetector.cpp
	UserInterfaceTask.cpp
	ImageTextRenderer.cpp
	ui/UserInterface.cpp
	ui/StubUserInterface.cpp
	ui/GLUserInterface.cpp
	Rect.cpp
	main.cpp
	VideoOutput.cpp
)

set(HDR_FILES
	utils/PosixCall.hpp
	utils/PosixCall.hpp
	utils/StringManipulation.hpp
	utils/Partitions.hpp
	utils/Slog.hpp
	Task.hpp
	FrameGrabber.hpp
	Connection.hpp
	StubFrameGrabber.hpp
	Options.hpp
	AcquisitionTask.hpp
	ProcessFrameTask.hpp
	ApriltagDetector.hpp
	UserInterfaceTask.hpp
	ImageTextRenderer.hpp
	ui/UserInterface.hpp
	ui/StubUserInterface.hpp
	ui/GLUserInterface.hpp
	Application.hpp
	Rect.hpp
	ImageU8.hpp
	VideoOutput.hpp
)

set(UTEST_SRC_FILES
	main-check.cpp #
	utils/StringManipulationUTest.cpp #
	ConnectionTest.cpp #
	utils/PartitionsUTest.cpp #
	OptionsUTest.cpp #
	TaskUTest.cpp
	TaskflowTest.cpp
	VideoOutputTest.cpp
)

set(UTEST_HDR_FILES
	utils/StringManipulationUTest.hpp #
	utils/PartitionsUTest.hpp #
	OptionsUTest.hpp #
	TaskUTest.hpp
)

if(EURESYS_FRAMEGRABBER_SUPPORT)
	list(APPEND SRC_FILES EuresysFrameGrabber.cpp)
	list(APPEND HDR_FILES EuresysFrameGrabber.hpp)
	set(FG_LIBRARIES ${EGRABBER_LIBRARIES})
endif(EURESYS_FRAMEGRABBER_SUPPORT)

if(HYPERION_FRAMEGRABBER_SUPPORT)
	list(APPEND SRC_FILES HyperionFrameGrabber.cpp)
	list(APPEND HDR_FILES HyperionFrameGrabber.hpp)
	set(FG_LIBRARIES Hyperion::mvImpact_Acquire)
endif(HYPERION_FRAMEGRABBER_SUPPORT)

if(MULTICAM_FRAMEGRABBER_SUPPORT)
	list(APPEND SRC_FILES MulticamFrameGrabber.cpp)
	list(APPEND HDR_FILES MulticamFrameGrabber.hpp)
	set(FG_LIBRARIES)
endif(MULTICAM_FRAMEGRABBER_SUPPORT)

add_library(artemis-common STATIC ${SRC_FILES} ${HDR_FILES})

add_resources(TARGET artemis-common RESOURCES ${RESOURCE_FILES})

target_link_libraries(
	artemis-common
	PUBLIC ${FG_LIBRARIES}
		   Eigen3::Eigen
		   fort-charis::libfort-time
		   fort-charis::libfort-utils
		   fort-charis::libfort-options
		   fort-charis::libfort-gl
		   fort-hermes::libfort-hermes
		   fort-tags::libfort-tags
		   "-ldl"
		   cpptrace::cpptrace
		   readerwriterqueue
		   taskflow::taskflow
		   libyuv::libyuv
		   spng
		   PkgConfig::gstreamer
)
add_executable(artemis main.cpp)

target_link_libraries(artemis artemis-common)

add_executable(artemis-tests ${UTEST_SRC_FILES} ${UTEST_HDR_FILES})
target_link_libraries(
	artemis-tests PUBLIC artemis-common GTest::gtest GTest::gmock
						 cpptrace::cpptrace
)
gtest_discover_tests(artemis-tests)
add_dependencies(check artemis-tests)
# add_check_test( NAME artemis FILES ${UTEST_SRC_FILES} ${UTEST_HDR_FILES}
# INCLUDE_DIRS ${GMOCK_INCLUDE_DIRS} LIBRARIES gmock artemis-common )

install(TARGETS artemis DESTINATION bin)
