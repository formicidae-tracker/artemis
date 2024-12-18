find_path(HYPERION_INCLUDE_DIR mvIMPACT_CPP/mvIMPACT_acquire.h
		  PATHS /opt/mvIMPACT_Acquire
)

find_library(
	HYPERION_LIBRARY mvHYPERIONfg
	PATHS /opt/mvIMPACT_Acquire
	PATH_SUFFIXES lib/x86_64
)

mark_as_advanced(HYPERION_LIBRARY HYPERION_INCLUDE_DIR)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
	Hyperion DEFAULT_MSG HYPERION_INCLUDE_DIR HYPERION_LIBRARY
)
if(Hyperion_FOUND)

	include(FetchContent)
	set(CLSER_LIBRARY_NAMES
		clserMV
		CACHE STRING "" FORCE
	)
	set(CLSER_LIBRARY_PATHS
		${HYPERION_INCLUDE_DIR}
		CACHE STRING "" FORCE
	)
	FetchContent_Declare(
		fort-clserpp
		GIT_REPOSITORY https://github.com/formicidae-tracker/clserpp.git
		GIT_TAG 0cf5a99
	)
	FetchContent_MakeAvailable(fort-clserpp)

	add_library(Hyperion::mv_ImpactAcquire INTERFACE IMPORTED)
	target_link_libraries(
		Hyperion::mv_ImpactAcquire INTERFACE ${HYPERION_LIBRARY}
											 fort-clserpp::clserpp
	)
	target_include_directories(
		Hyperion::mv_ImpactAcquire INTERFACE ${HYPERION_INCLUDE_DIR}
	)
endif(Hyperion_FOUND)
