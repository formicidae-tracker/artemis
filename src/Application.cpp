#include <chrono>
#include <climits>

#include <atomic>
#include <cpptrace/utils.hpp>
#include <ctime>
#include <execinfo.h>
#include <filesystem>
#include <linux/limits.h>
#include <pwd.h>

#include "Application.hpp"

#include "Options.hpp"

#include <Eigen/Core>

#include <opencv2/core.hpp>

#include <slog++/Config.hpp>
#include <slog++/Level.hpp>
#include <slog++/TeeSink.hpp>
#include <slog++/slog++.hpp>

#include <artemis-config.h>
#include <unistd.h>

#include "AcquisitionTask.hpp"
#include "FullFrameExportTask.hpp"
#include "ProcessFrameTask.hpp"
#include "UserInterfaceTask.hpp"
#include "VideoOutputTask.hpp"

namespace fort {
namespace artemis {

static std::once_flag handler_installed;

void printBacktrace(int signo, siginfo_t *info, void *context) {
	fprintf(stderr, "got SIGSEGV signal:\n");

	constexpr size_t MAX_STACKSIZE = 128;

	void *stack[MAX_STACKSIZE];
	auto  depth = backtrace(stack, MAX_STACKSIZE);
	auto  msg   = backtrace_symbols(stack, depth);
	for (int i = 0; i < depth; ++i) {
		fprintf(stderr, "[%d]: %s\n", i, msg[i]);
	}
	_exit(1);
}

void installCpptraceHandler() {
	cpptrace::register_terminate_handler();

	struct sigaction action = {0};
	action.sa_flags         = 0;
	action.sa_sigaction     = &printBacktrace;
	if (sigaction(SIGSEGV, &action, NULL) == -1) {
		perror("sigaction");
		_exit(1);
	}
}

void Application::Execute(int argc, char **argv) {
	std::call_once(handler_installed, installCpptraceHandler);

	Options options;
	options.SetDescription(
	    "run apriltag detection and coordinates with leto for results"
	);
	options.ParseArguments(argc, (const char **)argv);
	if (InterceptCommand(options) == true) {
		return;
	}

	InitLogging(argv[0], options);
	InitGlobalDependencies();

	Application application(options);
	application.Run();
};

bool Application::InterceptCommand(const Options &options) {
	if (options.PrintVersion == true) {
		std::cout << "artemis v" << ARTEMIS_VERSION << std::endl;
		return true;
	}

	if (options.PrintResolution == true) {
		auto resolution = AcquisitionTask::LoadFrameGrabber(
		                      options.StubImagePaths(),
		                      options.Camera
		)
		                      ->Resolution();
		std::cout << resolution.width() << " " << resolution.height()
		          << std::endl;
		return true;
	}

	return false;
}

struct LogfileName {
	std::string           name, hostname, username, timestamp;
	std::filesystem::path logpath;

	LogfileName(const char *argv0, const std::string &logdir) {
		if (logdir.empty()) {
			logpath = std::filesystem::current_path();
		} else {
			logpath = logdir;
		}
		std::filesystem::create_directories(logpath);

		auto now = std::chrono::system_clock::now();

		name = std::filesystem::path(argv0).filename();

		char hostname[HOST_NAME_MAX];
		if (gethostname(hostname, sizeof(hostname)) != 0) {
			this->hostname = hostname;
		} else {
			this->hostname = "unknown";
		}

		struct passwd *pw = getpwuid(getuid());
		username          = pw ? pw->pw_name : "unknown";

		auto               pid = getpid();
		std::ostringstream oss;
		auto               t = std::chrono::system_clock::to_time_t(now);
		std::tm            tm_time;
		localtime_r(&t, &tm_time);
		oss << std::put_time(&tm_time, "%Y%m%d-%H%M%S") << "." << pid;
		timestamp = oss.str();
	}

	static const char *levelname(slog::Level lvl) {

		switch (lvl) {
		case slog::Level::Trace:
			return "TRACE";
			break;
		case slog::Level::Debug:
			return "DEBUG";
			break;
		case slog::Level::Info:
			return "INFO";
			break;
		case slog::Level::Warn:
			return "WARNING";
			break;
		case slog::Level::Error:
		default:
			return "ERROR";
			break;
		}
	}

	std::string filename(slog::Level lvl) const {
		return name + "." + hostname + "." + username + ".log." +
		       levelname(lvl) + "." + timestamp;
	}

	std::string Get(slog::Level lvl) {
		auto linkpath   = logpath / (name + "." + levelname(lvl));
		auto targetpath = logpath / filename(lvl);
		std::filesystem::remove(linkpath);
		std::filesystem::create_symlink(targetpath, linkpath);
		return targetpath.string();
	}
};

void Application::InitLogging(const char *argv0, const Options &options) {

	LogfileName filenames{argv0, options.LogDir};

#ifndef NDEBUG
	auto stderr    = slog::BuildSink(slog::WithProgramOutput(
        slog::FromLevel(slog::Level::Debug),
        slog::WithFormat(slog::OutputFormat::TEXT)
    ));
	auto filedebug = slog::BuildSink(slog::WithFileOutput(
	    filenames.Get(slog::Level::Debug),
	    slog::FromLevel(slog::Level::Debug),
	    slog::WithFormat(slog::OutputFormat::JSON)
	));
#else
	auto stderr = slog::BuildSink();
#endif
	auto fileinfo  = slog::BuildSink(slog::WithFileOutput(
        filenames.Get(slog::Level::Info),
        slog::FromLevel(slog::Level::Info),
        slog::WithFormat(slog::OutputFormat::JSON)
    ));
	auto filewarn  = slog::BuildSink(slog::WithFileOutput(
        filenames.Get(slog::Level::Warn),
        slog::FromLevel(slog::Level::Warn),
        slog::WithFormat(slog::OutputFormat::JSON)
    ));
	auto fileerror = slog::BuildSink(slog::WithFileOutput(
	    filenames.Get(slog::Level::Error),
	    slog::FromLevel(slog::Level::Error),
	    slog::WithFormat(slog::OutputFormat::JSON)
	));

#ifndef NDEBUG
	slog::DefaultLogger().SetSink(
	    slog::TeeSink(stderr, fileerror, filewarn, fileinfo, filedebug)
	);
#else
	slog::DefaultLogger().SetSink(
	    slog::TeeSink(stderr, fileerror, filewarn, fileinfo)
	);

#endif
}

void Application::InitGlobalDependencies() {
	// Needed as we will do some parallelized access to Eigen ??
	Eigen::initParallel();
	// reduce the number of threads for OpenCV to allow some room for
	// other task (Display & IO)
	auto numThreads = cv::getNumThreads();
	if (numThreads == 2) {
		numThreads = 1;
	} else if (numThreads > 2) {
		numThreads -= 2;
	}
	cv::setNumThreads(numThreads);
}

Application::Application(const Options &options)
    : d_signals(d_context, SIGINT)
    , d_guard(d_context.get_executor()) {

	d_grabber = AcquisitionTask::LoadFrameGrabber(
	    options.StubImagePaths(),
	    options.Camera
	);

	d_process = std::make_shared<ProcessFrameTask>(
	    options,
	    d_context,
	    d_grabber->Resolution()
	);

	d_acquisition = std::make_shared<AcquisitionTask>(d_grabber, d_process);
}

void Application::SpawnTasks() {
	if (d_process->FullFrameExportTask()) {
		d_threads.push_back(Task::Spawn(*d_process->FullFrameExportTask(), 20));
	}

	if (d_process->VideoOutputTask()) {
		d_threads.push_back(Task::Spawn(*d_process->VideoOutputTask(), 0));
	}

	if (d_process->UserInterfaceTask()) {
		d_threads.push_back(Task::Spawn(*d_process->UserInterfaceTask(), 1));
	}

	d_threads.push_back(Task::Spawn(*d_process, 0));

	d_threads.push_back(Task::Spawn(*d_acquisition, 0));
}

void Application::JoinTasks() {
	// we join all subtask, but leave IO task alive
	for (auto &thread : d_threads) {
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
	d_signals.async_wait([this](const boost::system::error_code &, int) {
		slog::Info("Terminating (SIGINT)");
		d_acquisition->Stop();
	});
	// starts the context in a single threads, and remind to join it
	// once we got the SIGINT
	d_ioThread = std::thread([this]() {
		auto logger = slog::With(slog::String("task", "IO"));
		logger.Info("started");
		d_context.run();
		logger.Info("ended");
	});
}

void Application::Run() {
	SpawnIOContext();
	SpawnTasks();
	JoinTasks();
}

} // namespace artemis
} // namespace fort
