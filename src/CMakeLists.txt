
add_subdirectory(maytags)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})


include(CheckCXXSourceRuns)

Check_CXX_Source_Runs(
"
#include <system_error>


int main() {
	if (std::error_code(EDOM,std::system_category()) == std::errc::argument_out_of_domain ) {
		return 0;
	}
	return 1;
}
"
ARTEMIS_USE_SYSTEM_CATEGORY)


configure_file(config.h.in artemis-config.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR}
	                ${CMAKE_CURRENT_SOURCE_DIR}
	                )

add_subdirectory(hermes)
add_subdirectory(common)
add_subdirectory(utils)
add_subdirectory(orion)
add_subdirectory(artemis)
