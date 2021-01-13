find_path(EGRABBER_INCLUDE_DIR EGrabber.h
	      PATH_SUFFIXES include
	      PATHS /usr/local/opt/euresys/coaxlink
	            /opt/euresys/coaxlink
	            /opt/euresys/egrabber
	            )

mark_as_advanced(EGRABBER_INCLUDE_DIR)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(EGrabber DEFAULT_MSG EGRABBER_INCLUDE_DIR)

if(EGrabber_FOUND)
	set(EGRABBER_INCLUDE_DIRS ${EGRABBER_INCLUDE_DIR})

	include(CMakeFindDependencyMacro)
	find_dependency(Threads)
	set(EGRABBER_LIBRARIES Threads::Threads)

endif(EGrabber_FOUND)
