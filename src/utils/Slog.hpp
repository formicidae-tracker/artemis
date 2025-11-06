#pragma once

#include "Rect.hpp"
#include <slog++/Attribute.hpp>

namespace fort {
namespace artemis {

inline slog::Attribute slogRect(const char *name, const Rect &rect) {
	return slog::Group(
	    name,
	    slog::Int("x", rect.x()),
	    slog::Int("y", rect.y()),
	    slog::Int("width", rect.width()),
	    slog::Int("height", rect.height())
	);
}

} // namespace artemis
} // namespace fort
