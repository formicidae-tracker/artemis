include(FetchContent)
include(CMakeParseArguments)

function(fetch_fort_hermes)
	cmake_parse_arguments(OPTS "" "VERSION" "" ${ARGN})

	FetchContent_Declare(fort-hermes
	                     GIT_REPOSITORY https://github.com/formicidae-tracker/hermes.git
	                     GIT_TAG        ${OPTS_VERSION}
	                     )

	FetchContent_GetProperties(fort-hermes)
	if(NOT fort-hermes_POPULATED)
		FetchContent_Populate(fort-hermes)
		add_subdirectory(${fort-hermes_SOURCE_DIR} ${fort-hermes_BINARY_DIR})
	endif(NOT fort-hermes_POPULATED)

	set(FORT_HERMES_INCLUDE_DIRS ${fort-hermes_SOURCE_DIR}/src ${fort-hermes_BINARY_DIR}/src PARENT_SCOPE)

endfunction(fetch_fort_hermes)
