#pragma once
#include <cstddef>
#include <sys/wait.h>
#include <unistd.h>

#include <iostream>
#include <sstream>
#include <string>

#include <cpptrace/exceptions.hpp>
#include <utility>

namespace fort {
namespace artemis {
template <typename Str, typename... Args>
std::tuple<std::string, int> ExecAndCapture(Str &&filepath, Args &&...args) {

	int pipefd[2];
	if (pipe(pipefd) == -1) {
		throw cpptrace::runtime_error{"could not create pipe"};
	}

	pid_t pid = fork();
	if (pid == -1) {
		close(pipefd[0]);
		close(pipefd[1]);
		throw cpptrace::runtime_error{"fork failed"};
	}

	if (pid == 0) {
		dup2(pipefd[1], STDOUT_FILENO);
		dup2(pipefd[1], STDERR_FILENO);
		close(pipefd[0]);
		close(pipefd[1]);
		execlp(
		    std::forward<Str>(filepath),
		    std::forward<Str>(filepath),
		    std::forward<Args>(args)...,
		    nullptr
		);
		_exit(127);
	} else {
		close(pipefd[1]);
		std::ostringstream out;
		constexpr size_t   BUFFER_SIZE = 4096;
		char               buffer[BUFFER_SIZE];
		size_t             n;
		while ((n = read(pipefd[0], buffer, BUFFER_SIZE)) > 0) {
			out.write(buffer, n);
		}
		close(pipefd[0]);

		int  status = 0;
		auto w      = waitpid(pid, &status, 0);
		if (w == -1) {
			return {out.str(), -1};
		} else {
			if (WIFEXITED(status)) {
				return {out.str(), WEXITSTATUS(status)};

			} else if (WIFSIGNALED(status)) {

				return {out.str(), 128 + WTERMSIG(status)};
			} else {
				return {out.str(), -1};
			}
		}
	}
}

template <typename Str, typename... Args>
int Exec(Str &&filepath, Args &&...args) {
	pid_t pid = fork();
	if (pid == -1) {
		throw cpptrace::runtime_error{"fork failed"};
	}

	if (pid == 0) {
		std::cerr << filepath << " ";
		((std::cerr << std::forward<Args>(args) << " "), ...);
		std::cerr << std::endl;
		execlp(
		    std::forward<Str>(filepath),
		    std::forward<Str>(filepath),
		    std::forward<Args>(args)...,
		    nullptr
		);
		_exit(127);
	} else {
		return pid;
	}
}
} // namespace artemis
} // namespace fort
