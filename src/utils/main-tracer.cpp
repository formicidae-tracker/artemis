#include <cpptrace/basic.hpp>
#include <iostream>

#include <cpptrace/cpptrace.hpp>

int main(int argc, char **argv) {
	const char *filename = "artemistrace";
	if (argc >= 2) {
		filename = argv[1];
	}
	auto fd = fopen(filename, "r");
	if (fd == NULL) {
		std::cerr << "Could not open '" << filename << "'" << std::endl;
		return 1;
	}
	cpptrace::object_trace trace;
	int                    ret = 0;
	while (true) {
		cpptrace::safe_object_frame frame;
		std::size_t                 res = fread(&frame, sizeof(frame), 1, fd);
		if (res == 0) {
			break;
		} else if (res != 1) {
			std::cerr << "Something went wrong while reading from '" << filename
			          << "'" << std::endl;
			ret = 1;
			break;
		} else {
			trace.frames.push_back(frame.resolve());
		}
	}
	trace.resolve().print();
	return ret;
}
