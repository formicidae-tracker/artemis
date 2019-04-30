set(SRC_FILES main.cpp)

set(HDR_FILES )


add_executable(orion ${SRC_FILES} ${HDR_FILES})

target_link_libraries(orion maytags common)
