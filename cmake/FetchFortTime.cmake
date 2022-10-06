include(FetchContent)
include(CMakeParseArguments)

function(fetch_fort_time)
	cmake_parse_arguments(OPTS "" "VERSION" "" ${ARGN})

	FetchContent_Declare(fort-time
	                     GIT_REPOSITORY https://github.com/formicidae-tracker/time.git
	                     GIT_TAG        ${OPTS_VERSION}
	                     )

	FetchContent_GetProperties(fort-time)
	if(NOT fort-time_POPULATED)
		FetchContent_Populate(fort-time)
		add_subdirectory(${fort-time_SOURCE_DIR} ${fort-time_BINARY_DIR})
	endif(NOT fort-time_POPULATED)

	set(FORT_TIME_INCLUDE_DIRS ${fort-time_SOURCE_DIR}/src PARENT_SCOPE)
endfunction(fetch_fort_time)
