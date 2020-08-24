#include "Application.hpp"

#include "Options.hpp"

#include <glog/logging.h>
#include <Eigen/Core>

#include <opencv2/core.hpp>

#include <artemis-config.h>

#include "AcquisitionTask.hpp"
#include "ProcessFrameTask.hpp"
#include "FullFrameExportTask.hpp"

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
		auto resolution = AcquisitionTask::LoadFrameGrabber(options.General.StubImagePath,
		                                                    options.Camera)->Resolution();
		std::cout << resolution.width << " " << resolution.height << std::endl;
		return true;
	}

	return false;
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
	d_grabber = AcquisitionTask::LoadFrameGrabber(options.General.StubImagePath,
	                                              options.Camera);

	if ( options.Process.NewAntOutputDir.empty() == false ) {
		d_fullFrameExport = std::make_shared<FullFrameExportTask>(options.Process.NewAntOutputDir);
	}

	d_process = std::make_shared<ProcessFrameTask>(options,
	                                               nullptr,
	                                               nullptr,
	                                               nullptr,
	                                               d_fullFrameExport,
	                                               d_grabber->Resolution());



	d_acquisition = std::make_shared<AcquisitionTask>(d_grabber,d_process);


}

Application * Application::d_application = nullptr;

void Application::OnSigInt(int sig) {
	if ( d_application == nullptr ) {
		throw std::logic_error("Signal handler called without object");
		exit(1);
	}
	d_application->d_acquisition->Stop();
}

void Application::SpawnTasks() {
	if ( d_fullFrameExport ) {
		d_threads.push_back(Task::Spawn(*d_fullFrameExport,20));
	}
	d_threads.push_back(Task::Spawn(*d_process,0));
	d_threads.push_back(Task::Spawn(*d_acquisition,0));
}

void Application::JoinTasks() {
	for ( auto & thread : d_threads ) {
		thread.join();
	}
}

void Application::InstallSigIntHandler() {
	if ( d_application != nullptr ) {
		throw std::logic_error("Handler is already installed");
	}
	d_application = this;
	signal(SIGINT,&OnSigInt);
}


void Application::RemoveSigIntHandler() {
	signal(SIGINT,SIG_DFL);
	d_application = nullptr;
}

void Application::Run() {
	InstallSigIntHandler();
	SpawnTasks();
	JoinTasks();
	RemoveSigIntHandler();
}


} // namespace artemis
} // namespace fort
