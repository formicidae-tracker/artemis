#pragma once

#include <string>
#include <functional>
#include <algorithm>
#include <codecvt>

namespace base {

// Trims left element that matches a predicate
// @s the <std::string> to modify
// @predicate a function that should return true if the char should be trimmed
//
// @return a reference to <so>
std::string & TrimLeftFunc(std::string & s, std::function<bool (char )> predicate);


// Trims right element that matches a predicate
// @s the <std::string> to modify
// @predicate a function that should return true if the char should be trimmed
//
// @return a reference to <s>
std::string & TrimRightFunc(std::string & s, std::function<bool (char )> predicate);

// Trims left and righ elements that matches a predicate
// @s the <std::string> to modify
// @predicate a function that should return true if the char should be trimmed
//
// @return a reference to <s>
std::string & TrimFunc(std::string & s, std::function<bool (char )> predicate);

// Trims left char that matches a set of elements
// @s the <std::string> to modify
// @cutset a <std::string> of element that may be removed
//
// @return a reference to <s>
std::string & TrimLeftCutset(std::string & s, const std::string & cutset);

// Trims right char that matches a set of elements
// @s the <std::string> to modify
// @cutset a <std::string> of element that may be removed
//
// @return a reference to <s>
std::string & TrimRightCutset(std::string & s, const std::string & cutset);


// Trims right and left char that matches a set of elements
// @s the <std::string> to modify
// @cutset a <std::string> of element that may be removed
//
// @return a reference to <s>
std::string & TrimCutset(std::string & s, const std::string & cutset);


// Tests if string has a given prefix
// @s the string to test
// @prefix the prefix to test for
//
// @return true if the <s> starts with <prefix>
bool HasPrefix(const std::string & s, const std::string & prefix);

// Tests if string has a given suffix
// @s the string to test
// @suffix the suffix to test for
//
// @return true if the <s> ends with <suffix>
bool HasSuffix(const std::string & s, const std::string & suffix);

// Trims a prefix from a string
// @s the string to trim
// @prefix the prefix to remove from <s>
//
// @return a reference to <s>
std::string & TrimPrefix(std::string & s, const std::string & prefix);

// Trims a suffix from a string
// @s the string to trim
// @prefix the prefix to remove from <s>
//
// @return a reference to <s>
std::string & TrimSuffix(std::string & s, const std::string & suffix);


// Trims spaces of a string
// @s the strings to modify
//
// Is equivalent to `TrimFunc(s,[](char c){ std::isspace(c) != 0; });`
//
// @return a reference to <s>

std::string & TrimSpaces(std::string & s);


// Join a list of string
// @start the start of the range
// @end the end of the range to joint
// @separator the separator size
//
// @return a string of joined elements
template <typename InputIt>
std::string JoinString(const InputIt & start, const InputIt & end,
                       const std::string & separator) {

	size_t sum = (size_t)std::distance(start,end);
	if (sum <= 0 ) {
		return "";
	}
	sum *= separator.size();
	sum -= 1;
	for ( auto it = start; it != end; ++it) {
		sum += it->size();
	}
	std::string res;
	res.reserve(sum);
	res = *start;
	for(auto it = start + 1; it != end; ++it) {
		res += separator;
		res += *it;
	}
	return res;
}

template <typename InputIt,typename OutputIt>
OutputIt SplitString(const InputIt & start, const InputIt & end,
                     const std::string & separator,
                     OutputIt iter) {
	if (separator.empty()) {
		for(InputIt it = start; it != end; ) {
			std::string::size_type l = std::codecvt_utf8<wchar_t>::length(it,end,1);
			*iter++ = std::string(it,it+l);
			it += l;
		}
		return iter;
	}

	InputIt subStart = start;
	std::string::size_type increment = separator.size();
	while( subStart != end ) {
		InputIt subEnd = std::search(subStart,end,separator.begin(),separator.end());
		*iter++ = std::string(subStart,subEnd);

		//make sure we do not go over end
		if (subEnd == end ) {
			subStart = end;
		} else {
			subStart = subEnd + increment;
			// maybe we reached the end
			if (subStart == end ) {
				*iter++ = "";
			}
		}
	}

	return iter;
}

} //namespace base
