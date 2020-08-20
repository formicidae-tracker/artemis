#include "Application.hpp"

#include "Options.hpp"

#include <glog/logging.h>
#include <Eigen/Core>

#include <artemis-config.h>

#include "StubFrameGrabber.h"
#ifndef FORCE_STUB_FRAMEGRABBER_ONLY
#include "EuresysFrameGrabber.h"
#endif // FORCE_STUB_FRAMEGRABBER

namespace fort {
namespace artemis {


void Application::Execute(int argc, char ** argv) {
	auto options = Options::Parse(argc,argv);
	if ( InterceptCommand(options) == true ) {
		return;
	}

	InitGoogleLogging(argv[0],options.General);
	InitGlobalDependencies();

	Application application(options);
	application.Run();
};

bool Application::InterceptCommand(const Options & options ) {
	if ( options.General.PrintVersion == true ) {
		std::cout << "artemis v" << ARTEMIS_VERSION << std::endl;
		return true;
	}

	if ( options.General.PrintResolution == true ) {
		auto resolution = LoadFramegrabber(options.General.StubImagePath,
		                                   options.Camera)->GetResolution();
		std::cout << resolution.first << " " << resolution.second << std::endl;
		return true;
	}

	return false;
}

std::shared_ptr<FrameGrabber> Application::LoadFramegrabber(const std::string & stubPath,
                                                            const CameraOptions & options) {
#ifndef FORCE_STUB_FRAMEGRABBER_ONLY
	if (stubPath.empty() ) {
		return std::make_shared<EuresysFrameGrabber>(options);
	} else {
		return std::make_shared<StubFrameGrabber>(stubPath);
	}
#else
	return std::make_shared<StubFrameGrabber>(opts.StubImagePath);
#endif


}


void Application::InitGoogleLogging(const std::string & applicationName,
                       const GeneralOptions & options) {
	if ( options.LogDir.empty() == false ) {
		FLAGS_log_dir = options.LogDir.c_str();
	}
	// FLAGS_stderrthreshold = 0; // maybe we should need less log
	::google::InitGoogleLogging(applicationName.c_str());
	::google::InstallFailureSignalHandler();
}

void Application::InitGlobalDependencies() {
	// Needed as we will do some parallelized access to Eigen ??
	Eigen::initParallel();
	// reduce the number of threads for OpenCV to allow some room for
	// other task (Display & IO)
	auto numThreads = cv::getNumThreads();
	if ( numThreads == 2 ) {
		numThreads = 1;
	} else if ( numThreads > 2 ) {
		numThreads -= 2;
	}
	cv::setNumThreads(numThreads);
}


Application::Application(const Options & options) {


}

void Application::Run() {
}


} // namespace artemis
} // namespace fort
