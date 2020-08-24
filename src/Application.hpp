#pragma once

#include "Options.hpp"
#include  "Task.hpp"

#include <memory>
#include <vector>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>


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

	Application(const Options & options);

	void Run();

	void SpawnIOContext();
	void SpawnTasks();
	void JoinTasks();


	boost::asio::io_context d_context;
	boost::asio::signal_set d_signals;


	std::shared_ptr<FrameGrabber>        d_grabber;
	std::shared_ptr<ProcessFrameTask>    d_process;
	std::shared_ptr<AcquisitionTask>     d_acquisition;


	std::vector<std::thread>          d_threads;



};

} // namespace artemis
} // namespace fort
