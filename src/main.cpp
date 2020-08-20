#include <glog/logging.h>

#include "Application.hpp"

int main(int argc, char** argv) {
	try {
		fort::artemis::Application::Execute(argc,argv);
	} catch ( const std::exception & e ) {
		LOG(ERROR) << "Unhandled exception: " << e.what();
		return 1;
	} catch(...) {
		LOG(ERROR) << "Unhandled unknown error";
		return 2;
	}
	return 0;
}
