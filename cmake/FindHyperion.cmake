find_path(HYPERION_INCLUDE_DIR mvIMPACT_CPP/mvIMPACT_acquire.h
		  PATHS /opt/mvIMPACT_Acquire
)

find_library(
	HYPERION_LIBRARY mvHYPERIONfg
	PATHS /opt/mvIMPACT_Acquire
	PATH_SUFFIXES lib/x86_64
)

find_library(
	DEVICE_MANAGER_LIBRARY mvDeviceManager
	PATHS /opt/mvIMPACT_Acquire
	PATH_SUFFIXES lib/x86_64
)

mark_as_advanced(HYPERION_LIBRARY HYPERION_INCLUDE_DIR DEVICE_MANAGER_LIBRARY)
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
		GIT_TAG 69b542681dd3e0d1204ff157de83840f6ee96f34
	)
	FetchContent_MakeAvailable(fort-clserpp)

	add_library(Hyperion::mvImpact_Acquire UNKNOWN IMPORTED GLOBAL)
	set_target_properties(
		Hyperion::mvImpact_Acquire PROPERTIES IMPORTED_LOCATION
											  ${HYPERION_LIBRARY}
	)
	target_link_libraries(
		Hyperion::mvImpact_Acquire INTERFACE ${DEVICE_MANAGER_LIBRARY}
		# fort-clserpp::clserpp
	)

	target_include_directories(
		Hyperion::mvImpact_Acquire INTERFACE ${HYPERION_INCLUDE_DIR}
	)
endif(Hyperion_FOUND)
