#pragma once

#define fu_DEFER_UNIQUE_NAME_INNER(a, b) a##b
#define fu_DEFER_UNIQUE_NAME(base, line) fu_DEFER_UNIQUE_NAME_INNER(base, line)
#define fu_DEFER_NAME                    fu_DEFER_UNIQUE_NAME(zz_defer, __LINE__)
#define defer                            auto fu_DEFER_NAME = fort::utils::details::Defer_void{} *[&]()

namespace fort {
namespace utils {
namespace details {
template <typename Lambda> struct Deferrer {
	Lambda lambda;

	~Deferrer() {
		lambda();
	};
};

struct Defer_void {};

template <typename Lambda>
Deferrer<Lambda> operator*(Defer_void, Lambda &&lambda) {
	return {lambda};
}

} // namespace details
} // namespace utils
} // namespace fort
