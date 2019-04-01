add_subdirectory(hermes)

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
	          FrameGrabber.cpp
	          ProcessDefinition.cpp
	          ResizeProcess.cpp
	          Apriltag2Process.cpp
	          RingBuffer.cpp
	          ProcessQueueExecuter.cpp
	          utils/PosixCall.cpp
	          utils/FlagParser.cpp
	          utils/StringManipulation.cpp
	          OutputProcess.cpp
	          Connection.cpp
	          )



set(HDR_FILES utils/Defer.h
	          utils/PosixCall.h
	          FrameGrabber.h
	          ProcessDefinition.h
	          ResizeProcess.h
	          Apriltag2Process.h
	          RingBuffer.h
	          ProcessQueueExecuter.h
	          utils/PosixCall.h
	          utils/FlagParser.h
	          utils/StringManipulation.h
	          OutputProcess.h
	          Connection.h
	          )


set(UTEST_SRC_FILES main-check.cpp
	                utils/DeferUTest.cpp
	                RingBufferUTest.cpp
	                utils/FlagParserUTest.cpp
	                utils/StringManipulationUTest.cpp
	                ConnectionUTest.cpp
	                )

set(UTEST_HDR_FILES utils/DeferUTest.h
	                RingBufferUTest.h
	                utils/FlagParserUTest.h
	                utils/StringManipulationUTest.h
	                ConnectionUTest.h
	                )

if(EGrabber_FOUND)
	list(APPEND SRC_FILES EuresysFrameGrabber.cpp)
	list(APPEND HDR_FILES EuresysFrameGrabber.h)
endif(EGrabber_FOUND)

add_library(artemis-common STATIC ${SRC_FILES} ${HDR_FILES})
target_link_libraries(artemis-common ${GLOG_LIBRARIES}
	                                 ${EGRABBER_LIBRARIES}
	                                 ${APRILTAG2_LIBRARIES}
	                                 hermes
	                                 ${OpenCV_LIBS}
	                                  "-ldl")

add_executable(artemis main.cpp)

target_link_libraries(artemis artemis-common)


add_check_test(NAME artemis
	           FILES ${UTEST_SRC_FILES} ${UTEST_HDR_FILES}
	           INCLUDE_DIRS ${GMOCK_INCLUDE_DIRS}
	           LIBRARIES ${GMOCK_LIBRARIES} artemis-common)
