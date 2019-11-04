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
	          AntCataloguerProcess.cpp
	          LegacyAntCataloguerProcess.cpp
	          OverlayWriter.cpp
	          WatermarkingProcess.cpp
	          DrawDetectionProcess.cpp
	          utils/CPUMap.cpp
	          utils/Partitions.cpp
	          StubFrameGrabber.cpp
	          FrameDisplayer.cpp
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
	          AntCataloguerProcess.h
	          LegacyAntCataloguerProcess.h
	          OverlayWriter.h
	          WatermarkingProcess.h
	          DrawDetectionProcess.h
	          utils/CPUMap.h
	          utils/Partitions.h
	          StubFrameGrabber.h
	          FrameDisplayer.h
	          )


set(UTEST_SRC_FILES main-check.cpp
	                utils/DeferUTest.cpp
	                RingBufferUTest.cpp
	                utils/FlagParserUTest.cpp
	                utils/StringManipulationUTest.cpp
	                ConnectionUTest.cpp
	                ProcessQueueExecuterUTest.cpp
	                utils/PartitionsUTest.cpp
	                )

set(UTEST_HDR_FILES utils/DeferUTest.h
	                RingBufferUTest.h
	                utils/FlagParserUTest.h
	                utils/StringManipulationUTest.h
	                ConnectionUTest.h
	                ProcessQueueExecuterUTest.h
	                utils/PartitionsUTest.h
	                )

if(EGrabber_FOUND)
	list(APPEND SRC_FILES EuresysFrameGrabber.cpp)
	list(APPEND HDR_FILES EuresysFrameGrabber.h)
endif(EGrabber_FOUND)


add_library(artemis-common STATIC ${SRC_FILES} ${HDR_FILES})
target_link_libraries(artemis-common ${GLOG_LIBRARIES}
	                                 ${EGRABBER_LIBRARIES}
	                                 fort-tags
	                                 fort-hermes-cpp
	                                 ${PROTOBUF_LIBRARIES}
	                                 ${OpenCV_LIBS}
	                                  "-ldl")

add_executable(artemis main.cpp)

target_link_libraries(artemis artemis-common)


add_check_test(NAME artemis
	           FILES ${UTEST_SRC_FILES} ${UTEST_HDR_FILES}
	           INCLUDE_DIRS ${GMOCK_INCLUDE_DIRS}
	           LIBRARIES ${GMOCK_LIBRARIES} artemis-common)

install(TARGETS artemis DESTINATION bin)
