include_directories(${APRILTAG3_INCLUDE_DIRS})
set(SRC_FILES main.cpp)

set(HDR_FILES )


add_executable(orion ${SRC_FILES} ${HDR_FILES})

target_link_libraries(orion ${APRILTAG3_LIBRARIES}
	                        common
	                        ${GLOG_LIBRARIES}
	                        ${OpenCV_LIBRARIES})
