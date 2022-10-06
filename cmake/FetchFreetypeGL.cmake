include(FetchContent)
include(CMakeParseArguments)

function(fetch_freetype_gl)
	cmake_parse_arguments(OPTS "" "VERSION" "" ${ARGN})
	message(STATUS "Fetching freetype-gl version: ${OPTS_VERSION}")
	FetchContent_Declare(freetype-gl
	                     GIT_REPOSITORY https://github.com/rougier/freetype-gl
	                     GIT_TAG        ${OPTS_VERSION}
	                     )


    FetchContent_GetProperties(freetype-gl)
    if(NOT freetype-gl_POPULATED)
	    FetchContent_Populate(freetype-gl)
	    set(freetype-gl_BUILD_DEMOS OFF)
	    set(freetype-gl_BUILD_APIDOC OFF)
	    set(freetype-gl_BUILD_MAKEFONT OFF)
	    set(freetype-gl_BUILD_TESTS OFF)

		add_subdirectory(${freetype-gl_SOURCE_DIR} ${freetype-gl_BINARY_DIR})
    endif(NOT freetype-gl_POPULATED)
    # # manual target build
    # set(FREETYPE_GL_SRC texture-atlas.c
	#                     texture-font.c
	#                     distance-field.c
	#                     platform.c
	#                     utf8-utils.c
	#                     vector.c
	#                     edtaa3func.c
	#                     )
    # set(FREETYPE_GL_HDR texture-atlas.h
	#                     texture-font.h
	#                     distance-field.h
	#                     platform.h
	#                     utf8-utils.h
	#                     vector.h
	#                     edtaa3func.h
	#                     )
    # set(FREETYPE_GL_FILES )
    # foreach(F ${FREETYPE_GL_SRC})
	#     list(APPEND FREETYPE_GL_FILES ${freetype-gl_SOURCE_DIR}/${F})
    # endforeach(F ${FREETYPE_GL_SRC})
    # foreach(F ${FREETYPE_GL_HDR})
	#     list(APPEND FREETYPE_GL_FILES ${freetype-gl_SOURCE_DIR}/${F})
    # endforeach(F ${FREETYPE_GL_HDR})
    # add_library(freetype-gl STATIC ${FREETYPE_GL_FILES})
    set(FREETYPE_GL_INCLUDE_DIRS ${freetype-gl_SOURCE_DIR} ${freetype-gl_BINARY_DIR} PARENT_SCOPE)
endfunction(fetch_freetype_gl)
