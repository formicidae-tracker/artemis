include(FetchContent)
include(CMakeParseArguments)

function(fetch_boost_asio)
	cmake_parse_arguments(OPTS "" "VERSION" "" ${ARGN})

	FetchContent_Declare(boost-asio
	                     GIT_REPOSITORY https://github.com/boostorg/asio
	                     GIT_TAG        ${OPTS_VERSION}
	                     )

	FetchContent_GetProperties(boost-asio)
	if(NOT boost-asio_POPULATED)
		FetchContent_Populate(boost-asio)
	endif(NOT boost-asio_POPULATED)
	set(BOOST_ASIO_INCLUDE_DIRS ${boost-asio_SOURCE_DIR}/include PARENT_SCOPE)
	set(BOOST_ASIO_LIBRARIES boost_filesystem PARENT_SCOPE)
endfunction(fetch_boost_asio)
