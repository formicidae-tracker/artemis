set(SRC_FILES Defer.cpp
	          WaitGroup.cpp)

set(HDR_FILES Defer.h
	          WaitGroup.h)


set(UTEST_SRC_FILES main-check.cpp
	                DeferUTest.cpp
	                WaitGroupUTest.cpp)

set(UTEST_HDR_FILES DeferUTest.h
	                WaitGroupUTest.h)


add_library(artemis-common STATIC ${SRC_FILES} ${HDR_FILES})

add_executable(artemis main.cpp)

target_link_libraries(artemis artemis-common)


add_check_test(NAME artemis
	           FILES ${UTEST_SRC_FILES} ${UTEST_HDR_FILES}
	           INCLUDE_DIRS ${GMOCK_INCLUDE_DIRS}
	           LIBRARIES ${GMOCK_LIBRARIES} artemis-common)
