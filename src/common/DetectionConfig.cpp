#include "DetectionConfig.h"

#include <vector>
#include <map>
#include <stdexcept>

std::string DetectionConfig::FamilyName(FamilyType t) {
	static std::vector<std::string> names = {"36h11"};
	if ( (size_t)t > names.size() ) {
		throw std::invalid_argument("Unknown FamilyType");
	}
	return names[(size_t)t];
}

DetectionConfig::FamilyType DetectionConfig::FamilyTypeFromName(const std::string & name) {
	static std::map<std::string,FamilyType> types;
	if ( types.empty() ) {
		types["36h11"] = FamilyType::TAG36H11;
	}
	auto search = types.find(name);
	if ( search == types.end() ) {
		throw std::invalid_argument("Unknown family type '" + name + "'");
	}
	return search->second;

}
