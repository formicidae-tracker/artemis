#pragma once

#include "Options.hpp"
#include  "Task.hpp"

#include <memory>
#include <vector>



namespace fort {
namespace artemis {

class FrameGrabber;
class AcquisitionTask;
class ProcessFrameTask;
class FullFrameExportTask;

class Application {
public:
	static void Execute(int argc, char ** argv);



private :
	static bool InterceptCommand(const Options & options);

	static void InitGlobalDependencies();
	static void InitGoogleLogging(const std::string & applicationName,
	                              const GeneralOptions & options);

	static Application * d_application;
	static void OnSigInt(int sig);

	Application(const Options & options);

	void Run();
	void SpawnTasks();
	void JoinTasks();

	void InstallSigIntHandler();
	void RemoveSigIntHandler();


	std::shared_ptr<FrameGrabber>        d_grabber;
	std::shared_ptr<ProcessFrameTask>    d_process;
	std::shared_ptr<AcquisitionTask>     d_acquisition;
	std::shared_ptr<FullFrameExportTask> d_fullFrameExport;


	std::vector<std::thread>          d_threads;

};

} // namespace artemis
} // namespace fort
