#include "StringManipulation.h"

#include <algorithm>
#include <iostream>

namespace s=base;

std::string & s::TrimLeftFunc(std::string & s, std::function<bool (char )> predicate) {
	 s.erase(s.begin(),
	         std::find_if_not(s.begin(),
	                          s.end(),
	                          predicate));
	 return s;
}

std::string & s::TrimRightFunc(std::string & s, std::function<bool (char )> predicate) {
	s.erase(std::find_if_not(s.rbegin(),
	                         s.rend(),
	                         predicate).base(),
	        s.end());
	return s;
}

std::string & s::TrimFunc(std::string & s, std::function<bool (char )> predicate) {
	return TrimLeftFunc(TrimRightFunc(s,predicate),predicate);
}

std::string & s::TrimLeftCutset(std::string & s, const std::string & cutset) {
	return TrimLeftFunc(s,[&cutset](char c) {
			return cutset.find(c) != std::string::npos;
		});
}

std::string & s::TrimRightCutset(std::string & s,const std::string & cutset) {
	return TrimRightFunc(s,[&cutset](char c) {
			return cutset.find(c) != std::string::npos;
		});
}

std::string & s::TrimCutset(std::string & s, const std::string & cutset) {
	return TrimFunc(s,[&cutset](char c) {
			return cutset.find(c) != std::string::npos;
		});
}

bool s::HasPrefix(const std::string & s, const std::string & prefix) {
	return std::mismatch(prefix.begin(),prefix.end(),s.begin()).first == prefix.end();
}

bool s::HasSuffix(const std::string & s, const std::string & suffix) {
	return std::mismatch(suffix.rbegin(),suffix.rend(),s.rbegin()).first == suffix.rend();
}

std::string & s::TrimPrefix(std::string & s, const std::string & prefix) {
	if (!HasPrefix(s,prefix)) {
		return s;
	}
	s.erase(0,prefix.size());
	return s;
}

std::string & s::TrimSuffix(std::string & s, const std::string & suffix) {
	if ( !HasSuffix(s,suffix) ) {
		return s;
	}
	s.erase(s.size() - suffix.size());
	return s;
}

std::string & s::TrimSpaces(std::string & s) {
	return TrimFunc(s,[](char c) { return std::isspace(c);});
}

