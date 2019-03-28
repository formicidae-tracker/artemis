find_path(ASIO_INCLUDE_DIR asio/version.hpp)



if(ASIO_INCLUDE_DIR)
	file(STRINGS ${ASIO_INCLUDE_DIR}/asio/version.hpp ASIO_INCLUDE_VERSION
		 REGEX "^#define ASIO_VERSION")
	string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" ASIO_VERSION ${ASIO_INCLUDE_VERSION})
endif(ASIO_INCLUDE_DIR)


mark_as_advanced(ASIO_INCLUDE_DIR ASIO_VERSION ASIO_INCLUDE_VERSION)


include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Asio REQUIRED_VARS  ASIO_INCLUDE_DIR
                                                      ASIO_VERSION
                                       VERSION_VAR ASIO_VERSION)


if(Asio_FOUND)
	set(ASIO_INCLUDE_DIRS ${ASIO_INCLUDE_DIR})




endif(Asio_FOUND)
