#include "Application.hpp"
#include <slog++/slog++.hpp>

int main(int argc, char **argv) {
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
