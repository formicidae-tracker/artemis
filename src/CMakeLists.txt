include(CheckCXXSourceRuns)

Check_CXX_Source_Runs(
"
#include <system_error>


int main() {
	if (std::error_code(EDOM,std::system_category()) == std::errc::argument_out_of_domain ) {
		return 0;
	}
	return 1;
}
"
ARTEMIS_USE_SYSTEM_CATEGORY)


configure_file(config.h.in artemis-config.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR}
	                ${CMAKE_CURRENT_SOURCE_DIR}
	                )


set(SRC_FILES utils/Defer.cpp
	          utils/PosixCall.cpp
	          utils/FlagParser.cpp
	          utils/StringManipulation.cpp
	          utils/Partitions.cpp
	          Task.cpp
	          Time.cpp
	          Options.cpp
	          FrameGrabber.cpp
	          StubFrameGrabber.cpp
	          Connection.cpp
	          Application.cpp
	          AcquisitionTask.cpp
	          ProcessFrameTask.cpp
	          ApriltagDetector.cpp
	          FullFrameExportTask.cpp
	          UserInterfaceTask.cpp
	          VideoOutputTask.cpp
	          ImageTextRenderer.cpp
	          ui/UserInterface.cpp
	          ui/StubUserInterface.cpp
	          main.cpp
	          )


set(HDR_FILES utils/Defer.hpp
              utils/PosixCall.hpp
              utils/PosixCall.hpp
	          utils/FlagParser.hpp
	          utils/StringManipulation.hpp
	          utils/Partitions.hpp
	          Task.hpp
	          FrameGrabber.hpp
	          Time.hpp
	          Connection.hpp
	          StubFrameGrabber.hpp
	          Options.hpp
	          AcquisitionTask.hpp
	          ProcessFrameTask.hpp
	          ApriltagDetector.hpp
	          FullFrameExportTask.hpp
	          UserInterfaceTask.hpp
	          VideoOutputTask.hpp
	          ImageTextRenderer.hpp
	          ui/UserInterface.hpp
	          ui/StubUserInterface.hpp
	          Application.hpp
	          )


set(UTEST_SRC_FILES main-check.cpp
                    utils/DeferUTest.cpp
                    utils/FlagParserUTest.cpp
                    utils/StringManipulationUTest.cpp
                    TimeUTest.cpp
	                ConnectionUTest.cpp
	                utils/PartitionsUTest.cpp
	                OptionsUTest.cpp
	                TaskUTest.cpp
	                ObjectPoolUTest.cpp
	                )

set(UTEST_HDR_FILES utils/DeferUTest.hpp
	                utils/FlagParserUTest.hpp
	                utils/StringManipulationUTest.hpp
	                utils/PartitionsUTest.hpp
	                TimeUTest.hpp
	                ConnectionUTest.hpp
	                OptionsUTest.hpp
	                TaskUTest.hpp
	                ObjectPoolUTest.hpp
	                )

if(EGrabber_FOUND)
	list(APPEND SRC_FILES EuresysFrameGrabber.cpp)
	list(APPEND HDR_FILES EuresysFrameGrabber.hpp)
endif(EGrabber_FOUND)


add_library(artemis-common STATIC ${SRC_FILES} ${HDR_FILES})
target_link_libraries(artemis-common ${GLOG_LIBRARIES}
	                                 ${EGRABBER_LIBRARIES}
	                                 fort-tags
	                                 fort-hermes-cpp
	                                 ${PROTOBUF_LIBRARIES}
	                                 ${OpenCV_LIBS}
	                                 ${TBB_LIBRARIES}
	                                 glfw
	                                 boost_system
	                                  "-ldl")

add_executable(artemis main.cpp)

target_link_libraries(artemis artemis-common)


add_check_test(NAME artemis
	           FILES ${UTEST_SRC_FILES} ${UTEST_HDR_FILES}
	           INCLUDE_DIRS ${GMOCK_INCLUDE_DIRS}
	           LIBRARIES gmock artemis-common)

install(TARGETS artemis DESTINATION bin)
