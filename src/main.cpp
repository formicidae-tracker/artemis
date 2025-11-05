#include "Application.hpp"
#include <cpptrace/cpptrace.hpp>

#include <mutex>
#include <slog++/slog++.hpp>
#include <sys/wait.h>

static std::once_flag handler_installed;
static const char    *argv0 = nullptr;

// This is just a utility I like, it makes the pipe API more expressive.
struct pipe_t {
	struct int_tuple {
		int read_end;
		int write_end;
	};

	union {
		int_tuple tuple;
		int       data[2];
	};
};

void do_signal_safe_trace(cpptrace::frame_ptr *buffer, std::size_t count) {
	// Setup pipe and spawn child
	pipe_t input_pipe;
	pipe(input_pipe.data);
	const pid_t pid = fork();
	if (pid == -1) {
		const char *fork_failure_message = "fork() failed\n";
		write(
		    STDERR_FILENO,
		    fork_failure_message,
		    strlen(fork_failure_message)
		);
		return;
	}
	if (pid == 0) { // child
		dup2(input_pipe.tuple.read_end, STDIN_FILENO);
		close(input_pipe.tuple.read_end);
		close(input_pipe.tuple.write_end);
		const char *env[] = {"ARTEMIS_SIGNAL_TRACE=1", nullptr};
		execle(argv0, argv0, nullptr, env);
		const char *exec_failure_message =
		    "exec(artemis) failed: Make sure the signal_tracer "
		    "executable is in "
		    "the current working directory and the binary's permissions are "
		    "correct.\n";
		write(
		    STDERR_FILENO,
		    exec_failure_message,
		    strlen(exec_failure_message)
		);
		_exit(1);
	}
	// Resolve to safe_object_frames and write those to the pipe
	for (std::size_t i = 0; i < count; i++) {
		cpptrace::safe_object_frame frame;
		cpptrace::get_safe_object_frame(buffer[i], &frame);
		write(input_pipe.tuple.write_end, &frame, sizeof(frame));
	}
	close(input_pipe.tuple.read_end);
	close(input_pipe.tuple.write_end);
	// Wait for child
	waitpid(pid, nullptr, 0);
	_exit(1);
}

static std::once_flag backtrace_printed;

void print_backtrace_handler(int signo, siginfo_t *info, void *context) {
	// Print basic message
	const char *message = "SIGSEGV occurred:\n";
	write(STDERR_FILENO, message, strlen(message));
	// Generate trace
	constexpr static std::size_t N = 100;
	cpptrace::frame_ptr          buffer[N];
	std::size_t count = cpptrace::safe_generate_raw_trace(buffer, N);

	std::call_once(backtrace_printed, do_signal_safe_trace, buffer, count);
	// multiple segfault here.
	while (true) {
	}
}

void warmup_cpptrace() {
	// This is done for any dynamic-loading shenanigans
	static constexpr size_t BUFFER_SIZE = 10;
	cpptrace::frame_ptr     buffer[BUFFER_SIZE];
	cpptrace::safe_generate_raw_trace(buffer, BUFFER_SIZE);
	cpptrace::safe_object_frame frame;
	cpptrace::get_safe_object_frame(buffer[0], &frame);
}

void installCpptraceHandler(int argc, char **argv) {

	cpptrace::register_terminate_handler();

	argv0 = argv[0];
	warmup_cpptrace();
	struct sigaction action = {0};
	action.sa_flags         = 0;
	action.sa_sigaction     = &print_backtrace_handler;
	if (sigaction(SIGSEGV, &action, NULL) == -1) {
		perror("sigaction");
		_exit(1);
	}
}

void handle_signal_trace() {
	cpptrace::object_trace trace;
	while (true) {
		cpptrace::safe_object_frame frame;
		// fread used over read because a read() from a pipe might not read the
		// full frame
		std::size_t res = fread(&frame, sizeof(frame), 1, stdin);
		if (res == 0) {
			break;
		} else if (res != 1) {
			std::cerr << "Something went wrong while reading from the pipe"
			          << res << " " << std::endl;
			break;
		} else {
			trace.frames.push_back(frame.resolve());
		}
	}
	trace.resolve().print();
}

int main(int argc, char **argv) {

	if (getenv("ARTEMIS_SIGNAL_TRACE") != nullptr) {
		handle_signal_trace();
		return 0;
	}

	std::call_once(handler_installed, [argc, &argv]() {
		installCpptraceHandler(argc, argv);
	});

	try {
		fort::artemis::Application::Execute(argc, argv);
	} catch (const std::exception &e) {
		slog::Error("Unhandled exception: ", slog::Err(e));
		return 1;
	} catch (...) {
		slog::Error("Unhandled unknown error");
		return 2;
	}
	return 0;
}
