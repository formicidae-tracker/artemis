#include "CPUMap.h"

#include <fstream>

#include "PosixCall.h"

#include <map>
#include <algorithm>
#include "StringManipulation.h"

std::vector<Core> GetCoreMap() {
	std::ifstream cpuinfo("/proc/cpuinfo");
	if ( cpuinfo.is_open() == false ) {
		throw std::system_error(errno,ARTEMIS_SYSTEM_CATEGORY(),"Could not open cpuinfo: ");
	}

	std::map<CoreID,Core> resInMap;

	CPUID cpu = -1;
	for ( std::string line; getline(cpuinfo,line); ) {
		if ( base::HasPrefix(line, "processor") ) {
			std::string a = base::TrimPrefix(line, "processor");
			a = base::TrimSpaces(a);
			a = base::TrimPrefix(a, ":");
			a = base::TrimSpaces(a);
			cpu = atoi(a.c_str());
			continue;
		}

		if ( base::HasPrefix(line, "core id") ) {
			std::string a = base::TrimPrefix(line, "core id");
			a = base::TrimSpaces(a);
			a = base::TrimPrefix(a, ":");
			a = base::TrimSpaces(a);
			size_t coreid = atoi(a.c_str());
			if (cpu == -1 ) {
				throw std::runtime_error("core id without processor");
			}
			if ( resInMap.count(coreid) == 0 ) {
				resInMap[coreid] = Core{.ID=coreid};
			}
			resInMap[coreid].CPUs.push_back(cpu);
			cpu = -1;
			continue;
		}
	}

	std::vector<Core> res;
	res.reserve(resInMap.size());
	for( auto & kv : resInMap ) {
		std::sort(kv.second.CPUs.begin(),kv.second.CPUs.end());

		res.push_back(kv.second);

	}
	std::sort(res.begin(),res.end(),[](const Core & a, const Core & b)->bool {
			return a.ID < b.ID;
		});
	return res;
}
