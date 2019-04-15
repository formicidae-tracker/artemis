set(SRC_FILES Detector.cpp
	          Family.cpp
	          Implementation.cpp
	          Thresholder.cpp
	          ComponentConnecter.cpp
	          )


set(HDR_FILES Detector.h
	          Family.h
	          Implementation.h
	          Thresholder.h
	          ComponentConnecter.h
	          )


set(UTEST_HDR_FILES ThresholderUTest.h
	                ImageResource.h
	                ComponentConnecterUTest.h
	                )

set(UTEST_SRC_FILES ThresholderUTest.cpp
	                ImageResource.cpp
	                main-check.cpp
	                ComponentConnecterUTest.cpp
	                )

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/data)

set(RESSOURCES_FILES data/debug_threshold.pxm data/debug_preprocess.pxm data/debug_segmentation.pxm)

include(add_ressources)

add_resources(R ${RESSOURCES_FILES})

add_library(maytags SHARED ${SRC_FILES} ${HDR_FILES})


target_link_libraries(maytags ${OpenCV_LIBS})

add_check_test(NAME maytags
	           FILES ${UTEST_SRC_FILES} ${UTEST_HDR_FILES} ${R}
	           INCLUDE_DIRS ${GMOCk_INCLUDE_DIRS}
	           LIBRARIES ${GMOCK_LIBRARIES} maytags
	           )
