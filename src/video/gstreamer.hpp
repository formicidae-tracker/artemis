#pragma once

#include <gst/gst.h>

#include <gst/gstinfo.h>
#include <memory>
#include <sstream>

#include <cpptrace/cpptrace.hpp>

namespace fort {

namespace artemis {
// Ensure GST is intialized at least once.

template <typename T> struct GObjectUnrefer {
	void operator()(T *obj) const noexcept {
		if (obj != nullptr) {
			g_object_unref(obj);
		}
	}
};

template <typename T> struct GstBufferUnrefer {
	void operator()(T *obj) const noexcept {
		if (obj != nullptr) {
			gst_buffer_unref(obj);
		}
	}
};

template <typename T>
using glib_owned_ptr = std::unique_ptr<T, GObjectUnrefer<T>>;
using GstElementPtr  = glib_owned_ptr<GstElement>;
using GstBusPtr      = glib_owned_ptr<GstBus>;
using GstPadPtr      = glib_owned_ptr<GstPad>;
using GstBufferPtr   = std::unique_ptr<GstBuffer, GstBufferUnrefer<GstBuffer>>;
using GstElementRef  = GstElement *;
using GstBusRef      = GstBus *;

std::string GEnumToString(GType type, gint value);

void EnsureGSTInitialized();

namespace details {
template <typename T> std::string printValue(const T &value) {
	std::ostringstream oss;
	oss << std::boolalpha << value;
	return oss.str();
}

template <> inline std::string printValue<GValue>(const GValue &value) {
	return "unsupported";
}

template <typename Name, typename Value, typename... Other>
void append_pair(
    std::string &out, Name &&name, const Value &value, Other &&...others
) {
	static_assert(
	    sizeof...(Other) % 2 == 0,
	    "must have a pair number of name=value properties"
	);
	if (out.empty() == false) {
		out.push_back(' ');
	}

	out += std::string_view(std::forward<Name>(name));
	out += "=";
	out += printValue<Value>(value);

	if constexpr (sizeof...(Other) != 0) {
		append_pair(out, std::forward<Other>(others)...);
	}
}
} // namespace details

template <typename Str, typename... Args>
GstElement *GstElementFactory(Str &&name, Args &&...args) {
	EnsureGSTInitialized();
	static_assert(
	    sizeof...(Args) % 2 == 0,
	    "Must have an even number of parameters( pairs  name=value properties)"
	);
	GstElement *res = gst_element_factory_make_full(
	    std::forward<Str>(name),
	    std::forward<Args>(args)...,
	    nullptr
	);

	if (res == nullptr) {
		std::string params;
		details::append_pair(params, std::forward<Args>(args)...);
		throw cpptrace::runtime_error(
		    "could not create GstElement '" + std::string{name} + " " + params +
		    "'"
		);
	}
	return res;
}

} // namespace artemis
} // namespace fort
