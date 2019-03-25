include(CheckCXXSourceRuns)

Check_CXX_Source_Runs(
"
#include <system_error>


int main() {
	return std::error_code(EDOM,std::system_category()) == std::errc::argument_out_of_domain;
}
"
ARTEMIS_USE_SYSTEM_CATEGORY)


configure_file(config.h.in artemis-config.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})


set(SRC_FILES Defer.cpp
	          WaitGroup.cpp
	          EventManager.cpp
	          FrameGrabber.cpp
	          )

set(HDR_FILES Defer.h
	          WaitGroup.h
	          EventManager.h
	          utils/PosixCall.h
	          FrameGrabber.h
	          )


set(UTEST_SRC_FILES main-check.cpp
	                DeferUTest.cpp
	                WaitGroupUTest.cpp
	                EventManagerUTest.cpp
	                )

set(UTEST_HDR_FILES DeferUTest.h
	                WaitGroupUTest.h
	                EventManagerUTest.h
	                )


add_library(artemis-common STATIC ${SRC_FILES} ${HDR_FILES})
target_link_libraries(artemis-common ${GLOG_LIBRARIES}
	                                 ${EGRABBER_LIBRARIES}
	                                 ${APRILTAG2_LIBRARIES}
	                                 )


add_executable(artemis main.cpp)

target_link_libraries(artemis artemis-common)


add_check_test(NAME artemis
	           FILES ${UTEST_SRC_FILES} ${UTEST_HDR_FILES}
	           INCLUDE_DIRS ${GMOCK_INCLUDE_DIRS}
	           LIBRARIES ${GMOCK_LIBRARIES} artemis-common)
