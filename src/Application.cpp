#include "Application.hpp"

#include "Options.hpp"

#include <glog/logging.h>
#include <Eigen/Core>

#include <opencv2/core.hpp>

#include <artemis-config.h>

#include "AcquisitionTask.hpp"
#include "ProcessFrameTask.hpp"
#include "FullFrameExportTask.hpp"
#include "VideoOutputTask.hpp"
#include "UserInterfaceTask.hpp"

namespace fort {
namespace artemis {


void Application::Execute(int argc, char ** argv) {
	auto options = Options::Parse(argc,argv,true);
	if ( InterceptCommand(options) == true ) {
		return;
	}

	InitGoogleLogging(argv[0],options.General);
	InitGlobalDependencies(options.General);

	Application application(options);
	application.Run();
};

bool Application::InterceptCommand(const Options & options ) {
	if ( options.General.PrintVersion == true ) {
		std::cout << "artemis v" << ARTEMIS_VERSION << std::endl;
		return true;
	}

	if ( options.General.PrintResolution == true ) {
		auto resolution = AcquisitionTask::LoadFrameGrabber(options.General.StubImagePaths,
		                                                    options.Camera)->Resolution();
		std::cout << resolution.width << " " << resolution.height << std::endl;
		return true;
	}

	return false;
}


void Application::InitGoogleLogging(char * applicationName,
                                    const GeneralOptions & options) {
	if ( options.LogDir.empty() == false ) {
		// likely runned by leto and we are saving data to dedicated
		// files. So we do not need as much log to stderr, and no
		// color.
		FLAGS_log_dir = options.LogDir.c_str();
		FLAGS_stderrthreshold = 3;
		FLAGS_colorlogtostderr = false; // maybe we should need less log
	} else {
		// we output all logs, and likely we are not beeing runned by
		// leto, so we output everything with colors.
		FLAGS_stderrthreshold = 0;
		FLAGS_colorlogtostderr = true;
	}
	::google::InitGoogleLogging(applicationName);
	::google::InstallFailureSignalHandler();
}

void Application::InitGlobalDependencies(const GeneralOptions & options) {
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
	if ( options.NumberThreads > 0 ) {
		numThreads = options.NumberThreads;
	}



	cv::setNumThreads(numThreads);
}


Application::Application(const Options & options)
	: d_signals(d_context,SIGINT)
	, d_guard(d_context.get_executor()) {

	d_grabber = AcquisitionTask::LoadFrameGrabber(options.General.StubImagePaths,
	                                              options.Camera);

	d_process = std::make_shared<ProcessFrameTask>(options,
	                                               d_context,
	                                               d_grabber->Resolution());


	d_acquisition = std::make_shared<AcquisitionTask>(d_grabber,d_process);
}


void Application::SpawnTasks() {
	if ( d_process->FullFrameExportTask() ) {
		d_threads.push_back(Task::Spawn(*d_process->FullFrameExportTask(),20));
	}

	if ( d_process->VideoOutputTask() ) {
		d_threads.push_back(Task::Spawn(*d_process->VideoOutputTask(),0));
	}

	if ( d_process->UserInterfaceTask() ) {
		d_threads.push_back(Task::Spawn(*d_process->UserInterfaceTask(),1));
	}

	d_threads.push_back(Task::Spawn(*d_process,0));

	d_threads.push_back(Task::Spawn(*d_acquisition,0));
}

void Application::JoinTasks() {
	//we join all subtask, but leave IO task alive
	for ( auto & thread : d_threads ) {
		thread.join();
	}
	// now, nobody will post IO operation. we can reset safely.
	d_guard.reset();
	// we join the IO thread
	d_ioThread.join();
}

void Application::SpawnIOContext() {
	// putting a wait on SIGINT will ensure that the context remains
	// active throughout execution.
	d_signals.async_wait([this](const boost::system::error_code &,
	                            int ) {
		                     LOG(INFO) << "Terminating (SIGINT)";
		                     d_acquisition->Stop();
	                     });
	// starts the context in a single threads, and remind to join it
	// once we got the SIGINT
	d_ioThread = std::thread([this]() {
		                         LOG(INFO) << "[IOTask]: started";
		                         d_context.run();
		                         LOG(INFO) << "[IOTask]: ended";
	                         });
}

void Application::Run() {
	SpawnIOContext();
	SpawnTasks();
	JoinTasks();
}


} // namespace artemis
} // namespace fort
