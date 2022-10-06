include(FetchContent)
include(CMakeParseArguments)

function(fetch_fort_tags)
	cmake_parse_arguments(OPTS "" "VERSION" "" ${ARGN})
	FetchContent_Declare(fort-tags
	                     GIT_REPOSITORY https://github.com/formicidae-tracker/fort-tags.git
	                     GIT_TAG ${OPTS_VERSION}
	                     )

	FetchContent_GetProperties(fort-tags)
	if(NOT fort-tags_POPULATED)
		FetchContent_Populate(fort-tags)
		add_subdirectory(${fort-tags_SOURCE_DIR} ${fort-tags_BINARY_DIR})
	endif(NOT fort-tags_POPULATED)
	set(FORT_TAGS_INCLUDE_DIRS ${fort-tags_SOURCE_DIR}/src ${FETCHCONTENT_BASE_DIR}/src PARENT_SCOPE)

endfunction(fetch_fort_tags)
