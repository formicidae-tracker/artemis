#pragma once

#include <vector>
#include <cstddef>

typedef size_t CoreID;
typedef size_t CPUID;

struct Core {
	CoreID ID;
	std::vector<CPUID> CPUs;
};


std::vector<Core> GetCoreMap();
