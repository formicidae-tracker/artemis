#pragma once

#include <glib.h>
#include <memory>
#include <thread>
#include <vector>

#include "Options.hpp"

namespace fort {
namespace artemis {

class FrameGrabber;
class AcquisitionTask;
class ProcessFrameTask;
class FullFrameExportTask;

class Application {
public:
	static void Execute(int argc, char **argv);

private:
	static bool interceptCommand(const Options &options);

	static void initGlobalDependencies();
	static void initLogging(const char *argv0, const Options &options);

	static int onSigint(void *self);

	Application(const Options &options);
	~Application();
	Application(const Application &other)            = delete;
	Application(Application &&other)                 = delete;
	Application &operator=(const Application &other) = delete;
	Application &operator=(Application &&other)      = delete;

	void run();

	void spawnTasks();
	void joinTasks();
	void workgroupAdd(int i);

	std::shared_ptr<FrameGrabber>     d_grabber;
	std::shared_ptr<ProcessFrameTask> d_process;
	std::shared_ptr<AcquisitionTask>  d_acquisition;
	std::atomic<int>                  d_workgroup = 0;

	std::vector<std::thread> d_threads;
	GMainLoop               *d_loop;
};

} // namespace artemis
} // namespace fort
