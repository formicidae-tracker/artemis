include(FetchContent)
include(CMakeParseArguments)

function(fetch_google_test)
	cmake_parse_arguments(OPTS "" "VERSION" "" ${ARGN})
	FetchContent_Declare(googletest
	                     GIT_REPOSITORY https://github.com/google/googletest.git
	                     GIT_TAG        ${OPTS_VERSION}
	                     )

    FetchContent_GetProperties(googletest)
    if(NOT googletest_POPULATED)
	    FetchContent_Populate(googletest)
	    add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR} EXCLUDE_FROM_ALL)
    endif(NOT googletest_POPULATED)
endfunction(fetch_google_test)
