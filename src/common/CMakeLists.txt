set(SRC_FILES InterprocessBuffer.cpp
	          DetectionConfig.cpp)

set(HDR_FILES InterprocessBuffer.h
	          DetectionConfig.h)

add_library(common STATIC ${SRC_FILES} ${HDR_FILES})

target_link_libraries(common -lrt Threads::Threads)
