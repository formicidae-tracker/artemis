#pragma once

#include <vector>
#include <string>
#include <chrono>
#include <iostream>

namespace maytags {

class Profiler {
public:
	Profiler() : d_start(clock::now()) {}
	void Add(const std::string & label) {
		d_times.push_back(std::make_pair(label,clock::now()));
	}
	void Report(std::ostream & out) {
		clock::time_point last = d_start;
		for(auto const & tp : d_times ) {
			out << tp.first << " self: " << std::chrono::duration_cast<std::chrono::microseconds>(tp.second-last).count()
			    << "us total: " << std::chrono::duration_cast<std::chrono::microseconds>(tp.second-d_start).count()
			    << "us" << std::endl;
			last = tp.second;
		}
	}

private:
	typedef std::chrono::high_resolution_clock clock;
	clock::time_point d_start;
	std::vector<std::pair<std::string,clock::time_point> > d_times;

};


}
