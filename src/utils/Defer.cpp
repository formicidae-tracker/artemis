#include "Defer.hpp"



Defer::Defer(const std::function<void()> & toDefer)
	: d_toDefer(toDefer) {}

Defer::~Defer() {
	d_toDefer();
}
