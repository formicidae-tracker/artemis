#include <chrono>
#include <climits>

#include <cpptrace/utils.hpp>
#include <ctime>
#include <execinfo.h>
#include <filesystem>

#include <glib-unix.h>
#include <glib.h>

#include <pwd.h>

#include "Application.hpp"

#include "Options.hpp"

#include <Eigen/Core>

#include <slog++/Config.hpp>
#include <slog++/Level.hpp>
#include <slog++/TeeSink.hpp>
#include <slog++/slog++.hpp>

#include <artemis-config.h>
#include <unistd.h>

#include "AcquisitionTask.hpp"
#include "ProcessFrameTask.hpp"
#include "UserInterfaceTask.hpp" // IWYU pragma: keep

namespace fort {
namespace artemis {

void Application::Execute(int argc, char **argv) {
	Options options;
	options.SetDescription(
	    "run apriltag detection and coordinates with leto for results"
	);
	options.ParseArguments(argc, (const char **)argv);
	if (interceptCommand(options) == true) {
		return;
	}

	initLogging(argv[0], options);
	initGlobalDependencies();

	Application application(options);
	application.run();
};

bool Application::interceptCommand(const Options &options) {
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
		auto targetpath = filename(lvl);
		std::filesystem::remove(linkpath);
		std::filesystem::create_symlink(targetpath, linkpath);
		return (logpath / targetpath).string();
	}
};

void Application::initLogging(const char *argv0, const Options &options) {

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

void Application::initGlobalDependencies() {
	// Needed as we will do some parallelized access to Eigen ??
	Eigen::initParallel();
	// reduce the number of threads for OpenCV to allow some room for
	// other task (Display & IO)
	auto numThreads = std::thread::hardware_concurrency();
	if (numThreads == 2) {
		numThreads = 1;
	} else if (numThreads > 2) {
		numThreads -= 2;
	}
}

Application::~Application() {
	g_main_loop_unref(d_loop);
}

Application::Application(const Options &options)
    : d_loop{g_main_loop_new(nullptr, FALSE)} {

	d_grabber = AcquisitionTask::LoadFrameGrabber(
	    options.StubImagePaths(),
	    options.Camera
	);

	d_process = std::make_shared<ProcessFrameTask>(
	    options,
	    nullptr,
	    d_grabber->Resolution()
	);

	d_acquisition = std::make_shared<AcquisitionTask>(d_grabber, d_process);

	g_unix_signal_add(SIGINT, Application::onSigint, this);
}

void Application::workgroupAdd(int i) {
	auto old = d_workgroup.fetch_add(i);
	if ((old + i) == 0) {
		g_main_loop_quit(d_loop);
	}
}

void Application::spawnTasks() {
	auto onDone = [this]() { workgroupAdd(-1); };

	if (d_process->UserInterfaceTask()) {
		workgroupAdd(1);
		d_threads.push_back(
		    Task::Spawn(*d_process->UserInterfaceTask(), 1, onDone)
		);
	}

	workgroupAdd(1);
	d_threads.push_back(Task::Spawn(*d_process, 0, onDone));
	workgroupAdd(1);
	d_threads.push_back(Task::Spawn(*d_acquisition, 0, onDone));
}

void Application::joinTasks() {
	// we join all subtask, but leave IO task alive
	for (auto &thread : d_threads) {
		thread.join();
	}
}

int Application::onSigint(void *self_) {
	auto self = reinterpret_cast<Application *>(self_);
	slog::Info("Terminating (SIGINT)");
	self->d_acquisition->Stop();
	return G_SOURCE_REMOVE;
}

void Application::run() {
	spawnTasks();

	auto logger = slog::With(slog::String("task", "GLibMainLoop"));
	logger.Info("started");
	g_main_loop_run(d_loop);
	logger.Info("done");
	joinTasks();
}

} // namespace artemis
} // namespace fort
