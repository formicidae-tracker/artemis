set(SRC_FILES Detector.cpp
	          Family.cpp
	          Implementation.cpp
	          )


set(HDR_FILES Detector.h
	          Family.h
	          Implementation.h
	          )


set(UTEST_SRC_FILES
	                )

set(UTEST_SRC_FILES main-check.cpp
	                )

add_library(maytags SHARED ${SRC_FILES} ${HDR_FILES})


target_link_libraries(maytags ${OpenCV_LIBS})

add_check_test(NAME maytags
	           FILES ${UTEST_SRC_FILES} ${UTEST_HDR_FILES}
	           INCLUDE_DIRS ${GMOCk_INCLUDE_DIRS}
	           LIBRARIES ${GMOCK_LIBRARIES} maytags
	           )
