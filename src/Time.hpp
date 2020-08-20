#pragma once

#include <cstdint>
#include <chrono>
#include <string>
#include <iostream>
#include <iomanip>
#include <memory>

#include <google/protobuf/timestamp.pb.h>

namespace fort {

namespace artemis {

// The time ellapsed between two <Time>
//
// A <myrmidon::Duration> could be negative. Why not using
// std::chrono::duration ?  The C++ comittee took more than 9 years
// before figuring out that people may want to convert "1m" to a
// duration. Since <myrmidon::Time> should be re-implemented with a
// strong [golang time](https://golang.org/pkg/time) inspiration, why
// not doing the same for the associated <myrmidon::Duration>.
//
// This class aims to replicate a go syntax. For example to represent
// one hour, 10 minute, one may write:
//
// ```c++
// Duration d = 1 * Duration::Hour + 10 * Duration::Minute;
// ```
//
//
class Duration {
public:
	// constructor by an ammount of nanosecond
	// @ns the number of nanosecond
	inline Duration(int64_t ns)
		: d_nanoseconds(ns) {}
	// Default constructor with a zero duration.
	inline Duration()
		: d_nanoseconds(0) {
	}

	// constructor from std::chrono::duration
	// @T first <std::chrono::duration> template
	// @U second <std::chrono::duration> template
	// @duration the <std::chrono::duration> to convert
	template <typename T,typename U>
	Duration( const std::chrono::duration<T,U> & duration)
		: d_nanoseconds(std::chrono::duration<int64_t,std::nano>(duration).count()) {}


	// Gets the duration in hours
	//
	// R version:
	// ```R
	// d$hours()
	// ```
	//
	// @return the duration in hours
	double Hours() const;

	// Gets the duration in minutes
	//
	// R version:
	// ```R
	// d$minutes()
	// ```
	//
	// @return the duration in minutes
	double Minutes() const;

	// Gets the number of seconds
	//
	// R version:
	// ```R
	// d$seconds()
	// ```
	//
	// @return the duration in seconds
	double Seconds() const;

	// Gets the number of milliseconds
	//
	// R version:
	// ```R
	// d$milliseconds()
	// ```
	//
	// @return the duration in milliseconds
	double Milliseconds() const;

	// Gets the number of microseconds
	//
	// R version:
	// ```R
	// d$microseconds()
	// ```
	//
	// @return the duration in microseconds
	double Microseconds() const;

	// Gets the number of nanoseconds
	//
	// R version:
	// ```R
	// d$nanoseconds()
	// ```
	//
	// @return the duration in nanoseconds
	int64_t Nanoseconds() const {
		return d_nanoseconds;
	}

	// Parses a string to a Duration
	// @str the string to Parse in the form  `"2h"` or `"1m"`
	//
	// Parses a <std::string> to a <myrmidon::Duration>. string must be of the
	// form `[amount][unit]` where `[amount]` is a value that may
	// contain a decimal point, and `[unit]` could be any of `h`, `m`,
	// `s`, `ms`, `us`, `µs` and `ns`. This pattern may be
	// repeated. For example `4m32s` is a valid input.
	//
	// It may throw <std::exception> on any parsing error.
	//
	// @return the <myrmidon::Duration> represented by the string.
	static Duration Parse(const std::string & str);


	std::string ToString() const;

	// An hour
	//
	// Value for an hour.
	//
	// R version:
	// ```R
	// # we use a method to build an hour
	// fmHour(1)
	// ```
	const static Duration Hour;
	// A minute
	//
	// Value for a minute.
	//
	// R version:
	// ```R
	// # we use a method to build a minute
	// fmMinute(1)
	// ```
	const static Duration Minute;
	// A second
	//
	// Value for a second.
	//
	// R version:
	// ```R
	// # we use a method to build a second
	// fmSecond(1)
	// ```
	const static Duration Second;
	// A millisecond
	//
	// Value for a millisecond.
	//
	// R version:
	// ```R
	// # we use a method to build a millisecond
	// fmMillisecond(1)
	// ```
	const static Duration Millisecond;
	// A microsecond
	//
	// Value for a microsecond.
	//
	// R version:
	// ```R
	// # we use a method to build a microsecond
	// fmMicrosecond(1)
	// ```
	const static Duration Microsecond;
	// A Nanosecond
	//
	// Value for a nanosecond.
	//
	// R version:
	// ```R
	// # we use a method to build a nanosecond
	// fmNanosecond(1)
	// ```
	const static Duration Nanosecond;

	// The addition operator
	// @other the other <myrmidon::Duration> to add
	//
	// Adds two <myrmidon::Duration>.
	// @return a new duration `this + other `
	inline Duration operator+(const Duration & other) const {
		return d_nanoseconds + other.d_nanoseconds;
	}

	// Multiplication operator
	// @other the other <myrmidon::Duration> to multiply
	//
	// Multiplies two <myrmidon::Duration>.
	// @return a new duration `this * other `
	inline Duration operator*(const fort::artemis::Duration & other) const {
		return d_nanoseconds * other.d_nanoseconds;
	}

	// Soustraction operator
	// @other the other <myrmidon::Duration> to substract
	//
	// Substracts two <myrmidon::Duration>.
	// @return a new duration `this - other `
	inline Duration operator-(const fort::artemis::Duration & other) const {
		return d_nanoseconds - other.d_nanoseconds;
	}

	// Negation operator
	//
	// @return the opposite duration `- this`
	inline Duration operator-() const {
		return -d_nanoseconds;
	}

	// Less than operator
	// @other the <myrmidon::Duration> to compare
	//
	// Compares two <myrmidon::Duration>.
	// @return `this < other`
	inline bool operator<( const Duration & other ) const {
		return d_nanoseconds < other.d_nanoseconds;
	}

	// Less or equal operator
	// @other the <myrmidon::Duration> to compare
	//
	// Compares two <myrmidon::Duration>.
	// @return `this <= other`
	inline bool operator<=( const Duration & other ) const {
		return d_nanoseconds <= other.d_nanoseconds;
	}

	// Greater than operator
	// @other the <myrmidon::Duration> to compare
	//
	// Compares two <myrmidon::Duration>.
	// @return `this > other`
	inline bool operator>( const Duration & other ) const {
		return d_nanoseconds > other.d_nanoseconds;
	}

	// Greate or equal operator
	// @other the <myrmidon::Duration> to compare
	//
	// Compares two <myrmidon::Duration>.
	// @return `this >= other`
	inline bool operator>=( const Duration & other ) const {
		return d_nanoseconds >= other.d_nanoseconds;
	}

	// Equality operator
	// @other the <myrmidon::Duration> to compare
	//
	// Compares two <myrmidon::Duration>.
	// @return `this == other`
	inline bool operator==( const Duration & other ) const {
		return d_nanoseconds == other.d_nanoseconds;
	}


private:
	friend class Time;

	int64_t d_nanoseconds;
};


// A point in time
//
// <myrmidon::Time> represents a point in time. Why re-implementing
// one of this object and not using a `struct timeval` or a
// `std::chrono` ? We are dealing with long living experiment on
// heterogenous systems. Under this circunstances, we would like also
// to measure precise time difference. For this purpose we could use
// the framegrabber monolotic clock, which is timestamping every frame
// we acquired.
//
// Well the issue is that we cannot solely rely on this clock, as we
// may have several computers each with their own monolithic clock. Or
// even with a single computer, every time we started the tracking we
// must assume a new monotonic clock.
//
// We could use the wall clock but this clock may be resetted any
// time, and we would end up with issue where a time difference
// between two consecutive frames could be negative.
//
// Inspired from golang [time.Time](https://golang.org/pkg/time#Time)
// we propose an Object that store both a Wall time, and a Monotonic
// timestamp. But here we could have different monotonic timestamp. We
// try, whenever its possible (both <myrmidon::Time> have a monotonic
// time, and they are issued from the same monotonic clock), use that
// value for time differences and comparisons. Otherwise the Wall
// clock value will be used with the issue regarding the jitter or
// Wall clock reset.
//
// Differentiaing Monotonic clock is done through <MonoclockID>
// values. The 0 value is reserved for the <SYSTEM_MONOTONIC_CLOCK>
// and which is used by <Now>. When reading saved monotonic Timestamp
// from the filesystem (as it is the case when reading data from
// different `TrackingDataDirectory` ), care must be taken to assign
// different <MonoclockID> for each of those reading. This class does
// not enforce any mechanism. The only entry point to define the
// <MonoclockID> is through the utility function
// <FromTimestampAndMonotonic>.
//
// Every time are considered UTC.
class Time {
public:
	// A pointer to a Time
	typedef std::shared_ptr<Time>       Ptr;

	// A const pointer to a Time
	typedef std::shared_ptr<const Time> ConstPtr;

	// An object optimized for std::map or std::set keys
	//
	// <SortableKey> is an object constructed from current <Time> to be
	// used as a computationnaly efficient key in std::map or
	// std::set. please note that this key has lost any monotonic
	// information.
	typedef std::pair<int64_t,int32_t>  SortableKey;

	// Time values can overflow when performing operation on them.
	class Overflow : public std::runtime_error {
	public:
		// Construct an overflow from a clocktype name
		// @clocktype the clock type to use
		Overflow(const std::string & clocktype)
			: std::runtime_error(clocktype + " value will overflow") {}
		// default destructor
		virtual ~Overflow() {}
	};

	// ID for a Monotonic Clock
	typedef uint32_t MonoclockID;

	// Gets the current Time
	//
	// Gets the current <myrmidon::Time>. This time will both have a wall and a
	// monotonic clock reading associated with the
	// <SYSTEM_MONOTONIC_CLOCK>. Therefore such idioms:
	//
	// ```
	// Time start = Time::Now();
	// SomeFunction();
	// Duration ellapsed = Time::Now().Sub(start);
	// ```
	//
	// Will always return a positive <Duration>, even if the wall clock
	// has been reset between the two calls to <Now>
	//
	// R version:
	// ```R
	// t <- fmTimeNow()
	// ```
	//
	// @return the current <myrmidon::Time>
	static Time Now();

	static Time NowWithMonotonic(uint64_t nanoseconds, MonoclockID monoID);

	// Creates a Time from `time_t`
	// @t the time_t value
	//
	// Creates a <myrmidon::Time> from `time_t`. The <myrmidon::Time>
	// will not have any monotonic clock value.
	// @return the converted <myrmidon::Time>
	static Time FromTimeT(time_t t);

	// Creates a Time from `struct timeval`
	// @t the `struct timeval`
	//
	// Creates a <myrmidon::Time> from `struct timeval`. The
	// <myrmidon::Time> will not have any monotonic clock value.
	//
	// @return the converted <myrmidon::Time>
	static Time FromTimeval(const timeval & t);

	// Creates a Time from a protobuf Timestamp
	// @timestamp the `google.protobuf.Timestamp` message
	//
	// Creates a <myrmidon::Time> from a protobuf Timestamp. The
	// <myrmidon::Time> will not have any monotonic clock value.
	//
	// @return the converted <myrmidon::Time>
	static Time FromTimestamp(const google::protobuf::Timestamp & timestamp);

	// Creates a Time from a protobuf Timestamp and an external Monotonic clock
	// @timestamp the `google.protobuf.Timestamp` message
	// @nsecs the external monotonic value in nanoseconds
	// @monoID the external monoID
	//
	// Creates a <myrmidon::Time> from a protobuf Timestamp and an
	// external monotonic clock. The two values should correspond to
	// the same physical time. It is an helper function to create
	// accurate <myrmidon::Time> from data saved in
	// `fort.hermes.FrameReadout` protobuf messages that saves both a
	// Wall time value and a framegrabber timestamp for each frame. It
	// is the caller responsability to manage <monoID> value for not
	// mixing timestamp issued from different clocks. Nothing prevent
	// you to use <SYSTEM_MONOTONIC_CLOCK> for the <monoID> value but
	// the behavior manipulating resulting times is undefined.
	//
	// @return the converted <myrmidon::Time> with associated
	// monotonic data
	static Time FromTimestampAndMonotonic(const google::protobuf::Timestamp & timestamp,
	                                      uint64_t nsecs,
	                                      MonoclockID monoID);


	// Parses from RFC 3339 date string format.
	// @input the string to parse
	//
	// Parses from [RFC 3339](https://www.ietf.org/rfc/rfc3339.txt)
	// date string format, i.e. string of the form
	// `1972-01-01T10:00:20.021-05:00`. It is merely a wrapper from
	// google::protobuf::time_util functions.
	//
	// R version:
	// ```R
	// t <- fmTimeParse("1972-01-01T00:00:00.000Z")
	// ```
	//
	// @return the converted <myrmidon::Time>
	static Time Parse(const std::string & input);


	// Converts to a `time_t`
	//
	// Converts to a `time_t`. Please note that time_t have a maximal
	// resolution of a second.
	//
	// @return `time_t`representing the <myrmidon::Time>.
	time_t ToTimeT() const;

	// Converts to a `struct timeval`
	//
	// Converts to a `struct timeval`. Please note that time_t have a maximal
	// resolution of a microsecond.
	// @return `struct timeval`representing the <myrmidon::Time>.
	timeval ToTimeval() const;

	// Converts to a protobuf Timestamp message
	//
	// @return the protobuf Timestamp representing the <myrmidon::Time>.
	google::protobuf::Timestamp ToTimestamp() const;

	// In-place conversion to a protobuf Timestamp
	// @timestamp the timestamp to modify to represent the <myrmidon::Time>
	void ToTimestamp(google::protobuf::Timestamp * timestamp) const;

	// Zero time constructor
	Time();

	// Adds a Duration to a Time
	// @d the <Duration> to add
	//
	// R version:
	// ```R
	// newT <- t$add(fmSecond(1)) # a copy of t with one added second
	// ```
	//
	// @return a new <myrmidon::Time> distant by <d> from this <myrmidon::Time>
	Time Add(const Duration & d) const;

	// Rounds a Time to a Duration
	// @d the <Duration> to round to.
	//
	// Rounds the <myrmidon::Time> to the halp-rounded up <Duration>
	// d. Currently only multiple of <Duration::Second> and power of
	// 10 of <Duration::Nanosecond> which are smaller than a second
	// are supported.
	//
	// R version:
	// ```R
	// newT <- t$round(fmSecond(1)) # a copy of t rounded at the nearest second
	// ```
	//
	// @return a new <myrmidon::Time> rounded to the wanted duration
	Time Round(const Duration & d) const;

	// Reports if this time is after t
	// @t the <myrmidon::Time> to test against
	//
	// R version:
	// ```R
	// t1$after(t2) # return TRUE if t1 > t2
	// ```
	//
	// @return `true` if this <myrmidon::Time> is strictly after <t>
	bool After(const Time & t) const;

	// Reports if this time is before t
	// @t the <myrmidon::Time> to test against
	//
	// R version:
	// ```R
	// t1$before(t2) # return TRUE if t1 < t2
	// ```
	//
	// @return `true` if this <myrmidon::Time> is strictly before <t>
	bool Before(const Time & t) const;

	// Reports if this time is the same than t
	// @t the <myrmidon::Time> to test against
	//
	// R version:
	// ```R
	// t1$equals(t2) # return TRUE if t1 == t2
	// ```
	//
	// @return `true` if this <myrmidon::Time> is the same than <t>
	bool Equals(const Time & t) const;

	// Computes time difference with another time
	// @t the <myrmidon::Time> to substract to this one.
	//
	// R version:
	// ```R
	// # gets the fmDuration between t1 and t2
	// t1$sub(t2)
	// ```
	//
	// @return a <Duration> representing the time ellapsed between
	//         `this` and <t>. It could be negative.
	Duration Sub(const Time & t) const;

	// The current system monotonic clock.
	//
	// The <MonoclockID> reserved for the current system ( aka
	// `CLOCK_MONOTONIC`).
	const static MonoclockID SYSTEM_MONOTONIC_CLOCK = 0;

	// Reports the presence of a monotonic time value
	//
	// Reports the presence of a monotonic time value. Only
	// <myrmidon::Time> issued by <Now> or <FromTimestampAndMonotonic>
	// contains a monotonic time value.
	// @return `true` if `this` contains a monotonic clock value.
	bool HasMono() const;

	// Gets the referred MonoclockID.
	//
	// Gets the referred <MonoclockID>. It throws <std::exception> if
	// this <myrmidon::Time> has no monotonic clock value (see <HasMono>).
	// @return the <MonoclockID> designating the monotonic clock the
	//         monotonic time value refers to.
	MonoclockID MonoID() const;

	// Gets the monotonic value.
	//
	// Gets the monotonic value. It throws std::exception if this
	// <myrmidon::Time> has no monotonic clock value (see <HasMono>).
	// @return the monotonic clock value.
	uint64_t MonotonicValue() const;

	// Builds a debug string
	//
	// This method is useful for internal debugging. Prefer the
	// standard c++ formatting operator on <std::ostream>.
	//
	// @return a debug string with the complete time internal state.
	std::string DebugString() const;

	// Helpers to convert (sec,nsec) to nsec
	// @sec the amount of second
	// @nsec the amount of nanos
	//
	// Helpers to convert (sec,nsec) to nsec. Throws <Overflow> on
	// overflow.
	// @return sec * 1e9 + nsec if no overflow
	static uint64_t MonoFromSecNSec(uint64_t sec, uint64_t nsec);


	// Equal comparison operator
	// @other the other <myrmidon::Time> to compare to
	//
	// @return `true` if `this == other`
	inline bool operator == (const Time & other ) const  {
		return Equals(other);
	}

	// Less than operator
	// @other the other <myrmidon::Time> to compare to
	//
	// @return `true` if `this < other`
	inline bool operator < (const Time & other ) const  {
		return Before(other);
	}

	// Less or equal operator
	// @other the other <myrmidon::Time> to compare to
	//
	// @return `true` if `this <= other`
	inline bool operator <= (const Time & other ) const  {
		return !other.Before(*this);
	}

	// Greater than operator
	// @other the other <myrmidon::Time> to compare to
	//
	// @return `true` if `this > other`
	inline bool operator > (const Time & other ) const  {
		return other.Before(*this);
	}

	// Greate or equal operator
	// @other the other <myrmidon::Time> to compare to
	//
	// @return `true` if `this >= other`
	inline bool operator >= (const Time & other ) const  {
		return !Before(other);
	}

	// Builds a SortableKey for std::map and std::set.
	//
	// Builds a <SortableKey> representing this <myrmidon::Time>
	// suitable for <std::map> and <std::set>.
	// @return a <SortableKey> representing this <myrmidon::Time>
	inline SortableKey SortKey() const {
		return std::make_pair(d_wallSec,d_wallNsec);
	}

	// Builds a SortableKey representing a Time pointer
	// @timePtr the source <ConstPtr> to build a <SortableKey> for.
	//
	// Builds a <SortableKey> representing a <ConstPtr>. Passing a
	// nullptr to this function, will represent the smallest possible
	// key possible, which is then mathematically equivalent to -∞
	// <myrmidon::Time> (no result of <Parse> can represent this
	// value).
	// @return a <SortableKey> representing the <ConstPtr>
	static SortableKey SortKey(const ConstPtr & timePtr);

private:

	// Number of nanoseconds in a second.
	const static uint64_t NANOS_PER_SECOND = 1000000000ULL;
	// Number of nanoseconds in a millisecond.
	const static uint64_t NANOS_PER_MILLISECOND = 1000000ULL;
	// Number of nanoseconds in a microsecond.
	const static uint64_t NANOS_PER_MICROSECOND = 1000ULL;

	Time(int64_t wallsec, int32_t wallnsec, uint64_t mono, MonoclockID ID);

	Duration Reminder(const Duration & d) const;

	const static uint32_t HAS_MONO_BIT = 0x80000000ULL;
	int64_t     d_wallSec;
	int32_t     d_wallNsec;
	uint64_t    d_mono;
	MonoclockID d_monoID;
};


} // namespace myrmidon

} // namespace fort



// Operator for <fort::myrmidon::Duration> multiplication
// @a a signed integer
// @b the <fort::myrmidon::Duration> to multiply
//
// @return `a*b`
inline fort::artemis::Duration operator*(int64_t a,
                                          const fort::artemis::Duration & b) {
	return a * b.Nanoseconds();
}



// Formats a Duration
// @out the std::ostream to format to
// @d the <fort::myrmidon::Duration> to format
//
// Formats the <fort::myrmidon::Duration> to the form
// "1h2m3.4s". Leading zero unit are omitted, and unit smaller than 1s
// uses a smalle unit ms us or ns. The zero duration formats to 0s. It
// mimic golang's
// [time.Duration.String()](https://golang.org/pkg/time/#Duration.String)
// behavior.
//
// @return a reference to <out>

std::ostream & operator<<(std::ostream & out,
                          const fort::artemis::Duration & d);

// Formats to RFC 3339 date string format
// @out the output iostream
// @t the <fort::myrmidon::Time> to format
//
// Formats to [RFC 3339](https://www.ietf.org/rfc/rfc3339.txt) date
// string format, i.e. string of the form
// `1972-01-01T10:00:20.021Z`. It is merely a wrapper from
// google::protobuf::time_util functions.
//
// @return a reference to <out>
std::ostream & operator<<(std::ostream & out,
                          const fort::artemis::Time & t);

// Formats to RFC 3339 date string format
// @out the output iostream
// @t the <fort::myrmidon::Time::ConstPtr> to format
//
// Formats to [RFC 3339](https://www.ietf.org/rfc/rfc3339.txt) date
// string format, i.e. string of the form
// `1972-01-01T10:00:20.021Z`. It is merely a wrapper from
// google::protobuf::time_util functions.
//
// Please note that null pointer formats to `+/-∞`
//
// @return a reference to <out>
std::ostream & operator<<(std::ostream & out,
                          const fort::artemis::Time::ConstPtr & t );
