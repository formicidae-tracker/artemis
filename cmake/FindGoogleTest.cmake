find_path(GTEST_INCLUDE_DIR gtest/gtest.h
	                        HINTS $ENV{GTEST_ROOT}/include
	                              ${GTEST_ROOT}/include
	                        PATHS /usr/include
	                              /usr/src
	                        PATH_SUFFIXES googletest/googletest/include
	                        )

find_path(GTEST_SRC_DIR src/gtest-all.cc
	                    HINTS $ENV{GTEST_ROOT}/src
	                          ${GTEST_ROOT}/src
	                    PATHS /usr/src
	                    PATH_SUFFIXES googletest/googletest
	                    )

mark_as_advanced(GTEST_INCLUDE_DIR GTEST_SRC_DIR)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(GoogleTest DEFAULT_MSG GTEST_SRC_DIR GTEST_INCLUDE_DIR)

if(GoogleTest_FOUND)
	set(GTEST_INCLUDE_DIRS ${GTEST_INCLUDE_DIR})

	include(CMakeFindDependencyMacro)
	find_dependency(Threads)

	add_library(gtest SHARED ${GTEST_SRC_DIR}/src/gtest-all.cc)
	target_link_libraries(gtest Threads::Threads)
	target_include_directories(gtest PRIVATE ${GTEST_SRC_DIR} ${GTEST_INCLUDE_DIR})
	set(GTEST_LIBRARIES gtest)

endif(GoogleTest_FOUND)
