set(SRC_FILES FrameGrabber.cpp
	          ProcessDefinition.cpp
	          ResizeProcess.cpp
	          RingBuffer.cpp
	          OutputProcess.cpp
	          Connection.cpp
	          AntCataloguerProcess.cpp
	          DrawDetectionProcess.cpp
	          StubFrameGrabber.cpp
	          ChildProcess.cpp
	          ProcessQueueExecuter.cpp
	          )




set(HDR_FILES FrameGrabber.h
	          ProcessDefinition.h
	          ResizeProcess.h
	          RingBuffer.h
	          ProcessQueueExecuter.h
	          OutputProcess.h
	          Connection.h
	          AntCataloguerProcess.h
	          DrawDetectionProcess.h
	          StubFrameGrabber.h
	          ChildProcess.h
	          ProcessQueueExecuter.h
	          )



set(UTEST_SRC_FILES ../main-check.cpp
	                RingBufferUTest.cpp
	                ConnectionUTest.cpp
	                ProcessQueueExecuterUTest.cpp
	                )

set(UTEST_HDR_FILES RingBufferUTest.h
	                ConnectionUTest.h
	                ProcessQueueExecuterUTest.h
	                )

if(EGrabber_FOUND)
	list(APPEND SRC_FILES EuresysFrameGrabber.cpp)
	list(APPEND HDR_FILES EuresysFrameGrabber.h)
endif(EGrabber_FOUND)


add_library(artemis-common STATIC ${SRC_FILES} ${HDR_FILES})
target_link_libraries(artemis-common ${GLOG_LIBRARIES}
	                                 ${EGRABBER_LIBRARIES}
	                                 hermes
	                                 common
	                                 utils
	                                 ${OpenCV_LIBS}
	                                  "-ldl")


add_executable(artemis main.cpp)

target_link_libraries(artemis artemis-common)

add_check_test(NAME artemis
	           FILES ${UTEST_SRC_FILES} ${UTEST_HDR_FILES}
	           INCLUDE_DIRS ${GMOCK_INCLUDE_DIRS}
	           LIBRARIES ${GMOCK_LIBRARIES} artemis-common)
