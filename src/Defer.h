#pragma once

#include <functional>


class Defer {

public:
	Defer(const std::function<void()> & toDefer);
	~Defer();

private :
	const std::function<void()> & d_toDefer;
};
