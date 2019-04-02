find_path(GMOCK_INCLUDE_DIR gmock/gmock.h
	                        HINTS $ENV{GTEST_ROOT}/include
	                              ${GTEST_ROOT}/include
	                        PATHS /usr/include
	                              /usr/src
	                        PATH_SUFFIXES googletest/googlemock/include
	                        )

find_path(GMOCK_SRC_DIR src/gmock-all.cc
	                    HINTS $ENV{GTEST_ROOT}/src
	                          ${GTEST_ROOT}/src
	                    PATHS /usr/src
	                    PATH_SUFFIXES googletest/googlemock
	                    )

mark_as_advanced(GMOCK_INCLUDE_DIR GMOCK_SRC_DIR)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(GoogleMock DEFAULT_MSG GMOCK_SRC_DIR GMOCK_INCLUDE_DIR)

if(GoogleMock_FOUND)
	set(GMOCK_INCLUDE_DIRS ${GMOCK_INCLUDE_DIR})

	include(CMakeFindDependencyMacro)
	find_dependency(GoogleTest)

	add_library(gmock SHARED ${GMOCK_SRC_DIR}/src/gmock-all.cc)
	target_link_libraries(gmock gtest)
	target_include_directories(gmock PUBLIC ${GTEST_INCLUDE_DIRS} PRIVATE ${GMOCK_SRC_DIR} ${GMOCK_INCLUDE_DIR})
	set(GMOCK_LIBRARIES gmock)

endif(GoogleMock_FOUND)
