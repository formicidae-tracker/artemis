#include "Family.h"

#include <map>

namespace maytags {

Family::Ptr Family36h11();


Family::Ptr Family::Create(const std::string & name) {
	static std::map<std::string,std::function<Family::Ptr()> > factory;
	if ( factory.empty() ) {
		factory["36h11"] = Family36h11;
	}

	auto search = factory.find(name);

	if ( search == factory.end() ) {
		throw std::invalid_argument("Unknown family '" + name + "'");
	}
	return search->second();

}

}
