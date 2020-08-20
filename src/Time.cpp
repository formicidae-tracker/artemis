#include "Time.hpp"

#include <time.h>

#include <stdexcept>
#include <map>
#include <sstream>

#include "utils/PosixCall.hpp"

#include <google/protobuf/util/time_util.h>

namespace fort {
namespace artemis {

using nanos = std::chrono::duration<uint64_t,std::nano>;

const Duration Duration::Hour        = 3600e9;
const Duration Duration::Minute      = 60e9;
const Duration Duration::Second      = 1e9;
const Duration Duration::Millisecond = 1e6;
const Duration Duration::Microsecond = 1e3;
const Duration Duration::Nanosecond  = 1;


double Duration::Hours() const {
	return double(d_nanoseconds)/double(3600.0e9);
}

double Duration::Minutes() const {
	return double(d_nanoseconds)/double(60.0e9);
}

double Duration::Seconds() const {
	return double(d_nanoseconds)/double(1.0e9);
}

double Duration::Milliseconds() const {
	return double(d_nanoseconds)/double(1.0e6);
}

double Duration::Microseconds() const {
	return double(d_nanoseconds)/double(1.0e3);
}

Duration Duration::Parse(const std::string & i) {
	uint64_t integer(0);
	double frac(0);
	bool neg(false);

#define throws(c) throw std::runtime_error("Could not parse '" + i + "':" + c)
	if ( i.empty() ) {
		throws("empty");
	}

	auto it = i.cbegin();

	if (*it == '-' || *it == '+' ) {
		neg = *it == '-';
		++it;
	}

	if ( i.cend() - it == 1 && *it == '0' ) {
		return 0;
	}


	bool ok = *it == '.';
	bool zeroOK = false;
	for (;  it != i.cend() && *it >= '0' && *it <= '9'; ++it) {
		ok = true;
		zeroOK = true;
		integer = integer * 10 + (*it - '0');
	}

	if ( integer > std::numeric_limits<int64_t>::max() ) {
		throws("integer overflow");
	}

	if ( ok == false ) {
		throws("need a number");
	}


	if (it != i.cend() && *it == '.' ) {
		++it;
		for (double base = 0.1;  it != i.cend() && *it >= '0' && *it <= '9'; ++it) {
			frac += (*it - '0') * base;
			base /= 10.0;
		}
	}

	if ( integer == 0 && frac == 0.0 && zeroOK == false ) {
		throws("empty number");
	}

	std::string unit;
	for ( ;it != i.cend() && ( *it < '0' || *it > '9') ; ++it ) {
		unit += *it;
	}

	static std::map<std::string,int64_t> units
		= {
		   {"ns",Nanosecond.Nanoseconds()},
		   {"us",Microsecond.Nanoseconds()},
		   {"µs",Microsecond.Nanoseconds()},
		   {"μs",Microsecond.Nanoseconds()},
		   {"ms",Millisecond.Nanoseconds()},
		   {"s",Second.Nanoseconds()},
		   {"m",Minute.Nanoseconds()},
		   {"h",Hour.Nanoseconds()},
	};
	auto fi = units.find(unit);
	if ( fi == units.end() ) {
		throws("unknown unit '" + unit + "'");
	}
	if ( integer > std::numeric_limits<int64_t>::max() / fi->second ) {
		throws("integer will overflow");
	}
	int64_t res = integer * fi->second;
	//fractionnal part cannot overflow
	int64_t resfrac = frac * fi->second;

	if ( res > std::numeric_limits<int64_t>::max() - resfrac ) {
		throws("will overflow");
	}
	res += resfrac;

	if (it == i.cend()) {
		if ( neg == true ) { return -res; };
		return res;
	}
	int64_t other = Parse(std::string(it,i.cend())).Nanoseconds();

	if ( neg == true ) {
		if ( -res < std::numeric_limits<int64_t>::min() + other ) {
			throws("overflow");
		}
		return - res - other;
	}
	if ( res > std::numeric_limits<int64_t>::max() - other ) {
		throws("overflow");
	}

#undef throws
	return  res + other;
}

// we don't use numeric_limit as we want to force pre-compiled
// values for some low-level functions
#define MAX_UINT64 (uint64_t(0xffffffffffffffffULL))
#define MAX_SINT64 (int64_t(0x7fffffffffffffffLL))
#define MIN_SINT64 (int64_t(0x8000000000000000LL))
#define NANOS_PER_SECOND_UINT64 1000000000ULL
#define NANOS_PER_SECOND_SINT64 1000000000LL

#define MAX_SECOND_UINT64 uint64_t(MAX_UINT64 / NANOS_PER_SECOND_UINT64)
#define MAX_SECOND_SINT64 int64_t(MAX_SINT64 / NANOS_PER_SECOND_SINT64)
#define MIN_SECOND_SINT64 int64_t(MIN_SINT64 / NANOS_PER_SECOND_SINT64)



uint64_t Time::MonoFromSecNSec(uint64_t sec, uint64_t nsec) {

	if ( sec > MAX_SECOND_UINT64 ) {
		throw Overflow("Mono");
	}

	uint64_t res = sec * NANOS_PER_SECOND_UINT64;
	if ( res > MAX_UINT64 - nsec ) {
		throw Overflow("Mono");
	}

	return res + nsec;
}

Time Time::NowWithMonotonic(uint64_t nanoseconds, MonoclockID monoID) {
	struct timespec wall;
	p_call(clock_gettime,CLOCK_REALTIME,&wall);

	return Time(wall.tv_sec,wall.tv_nsec,
	            nanoseconds,
	            HAS_MONO_BIT | monoID);
}



Time Time::Now() {
	struct timespec wall,mono;
	p_call(clock_gettime,CLOCK_REALTIME,&wall);
	p_call(clock_gettime,CLOCK_MONOTONIC,&mono);

	return Time (wall.tv_sec, wall.tv_nsec,
	             MonoFromSecNSec(mono.tv_sec, mono.tv_nsec),
	             HAS_MONO_BIT | SYSTEM_MONOTONIC_CLOCK);
}

Time Time::FromTimeT(time_t value) {
	return Time(value,0 ,0,0);
}

time_t Time::ToTimeT() const {
	return d_wallSec;
}

Time Time::FromTimeval(const timeval & t) {
	return Time(t.tv_sec, t.tv_usec * NANOS_PER_MICROSECOND,0,0);
}

timeval Time::ToTimeval() const {
	timeval res;
	res.tv_sec = d_wallSec;
	res.tv_usec = d_wallNsec / NANOS_PER_MICROSECOND;
	return res;
}

Time Time::FromTimestamp(const google::protobuf::Timestamp & timestamp) {
	return Time(timestamp.seconds(), timestamp.nanos(), 0, 0);
}

google::protobuf::Timestamp Time::ToTimestamp() const {
	google::protobuf::Timestamp pb;
	ToTimestamp(&pb);
	return pb;
}


void Time::ToTimestamp(google::protobuf::Timestamp * timestamp) const {
	timestamp->set_seconds(d_wallSec);
	timestamp->set_nanos(d_wallNsec);
}

Time Time::FromTimestampAndMonotonic(const google::protobuf::Timestamp & timestamp,
                                     uint64_t nsecs,
                                     MonoclockID monoID) {
	// we test against *int32_t*  has last bi is a flag.
	if (monoID > uint32_t(std::numeric_limits<int32_t>::max()) ) {
		throw Overflow("MonoID");
	}
	return Time(timestamp.seconds(), timestamp.nanos(),
	            nsecs,
	            HAS_MONO_BIT | monoID);
}


Time Time::Parse(const std::string & input) {
	google::protobuf::Timestamp pb;
	if ( google::protobuf::util::TimeUtil::FromString(input,&pb) == false ) {
		throw std::runtime_error("Time: could not parse '" + input + "'");
	}
	return FromTimestamp(pb);
}

Time::Time()
	: d_wallSec(0)
	, d_wallNsec(0)
	, d_mono(0)
	, d_monoID(0) {
}

Time::Time(int64_t wallSec, int32_t wallNsec, uint64_t mono, MonoclockID monoID)
	: d_wallSec(wallSec)
	, d_wallNsec(wallNsec)
	, d_mono(mono)
	, d_monoID(monoID) {
	while(d_wallNsec >= NANOS_PER_SECOND_SINT64 ) {
		if (d_wallSec == MAX_SINT64 ) {
			throw Overflow("Wall");
		}
		++d_wallSec;
		d_wallNsec -= NANOS_PER_SECOND_SINT64;
	}

	while(d_wallNsec < 0) {
		if (d_wallSec == MIN_SINT64 ) {
			throw Overflow("Wall");
		}
		--d_wallSec;
		d_wallNsec += NANOS_PER_SECOND_SINT64;
	}
}

#define MONO_MASK ( HAS_MONO_BIT - 1 )

Time Time::Add(const Duration & d) const{
	uint64_t mono = d_mono;
	int64_t toAdd = d.Nanoseconds();
	if ( (d_monoID & HAS_MONO_BIT) != 0 ) {

		if ( (toAdd > 0 && d_mono  > MAX_UINT64 - toAdd) ||
		     (toAdd < 0 && -toAdd > d_mono ) ) {
			throw Overflow("Mono");
		}

		mono = d_mono + toAdd;
	}

	int64_t seconds = toAdd / NANOS_PER_SECOND_SINT64;
	int64_t nanos = toAdd - seconds * NANOS_PER_SECOND_SINT64;

	return Time(d_wallSec + seconds, d_wallNsec + nanos ,mono,d_monoID);
}




Time Time::Round(const Duration & d) const {
	auto res = * this;
	// strip mono data
	res.d_mono = 0;
	res.d_monoID = 0;
	if (d.d_nanoseconds < 0 ) {
		return res;
	}

	auto r = Reminder(d);
	if ( uint64_t(r.d_nanoseconds) + uint64_t(r.d_nanoseconds) < int64_t(d.d_nanoseconds) ) {
		return res.Add(-r);
	}
	return res.Add(d-r);
}

bool IsPowerOf10(int64_t value) {
	while( value > 9 && value % 10 == 0) {
		value /= 10;
	}
	return value == 1;
}


Duration Time::Reminder(const Duration & d) const {
	int64_t sec = d_wallSec;
	int64_t nsec = d_wallNsec;
	if ( sec < 0 ) {
		sec = -sec;
		nsec = -nsec;
		if ( nsec < 0 ) {
			nsec += NANOS_PER_SECOND;
			sec--;
		}
	}

	if ( d.d_nanoseconds % NANOS_PER_SECOND == 0) {
		int64_t dSec = d.d_nanoseconds / NANOS_PER_SECOND;
		return (sec % dSec) * NANOS_PER_SECOND + nsec;
	}

	// tests if we are a power of 10 of a nanoseconds and less than a second
	if ( d.d_nanoseconds < NANOS_PER_SECOND && IsPowerOf10(d.d_nanoseconds) == true ) {
		return nsec % d.d_nanoseconds;
	}

	throw std::runtime_error("This implementation only supports Duration that are multiple of a second or power of 10 of a nanosecond");
}


bool Time::After(const Time & t) const {
	if ( d_monoID != 0 && d_monoID == t.d_monoID) {
		return d_mono > t.d_mono;
	}
	if ( d_wallSec == t.d_wallSec ) {
		return d_wallNsec > t.d_wallNsec;
	}
	return d_wallSec > t.d_wallSec;
}

bool Time::Equals(const Time & t) const {
	if ( d_monoID != 0 && d_monoID == t.d_monoID) {
		return d_mono == t.d_mono;
	}
	return d_wallSec == t.d_wallSec && d_wallNsec == t.d_wallNsec;
}

bool Time::Before(const Time & t) const {
	if ( d_monoID != 0 && d_monoID == t.d_monoID) {
		return d_mono < t.d_mono;
	}
	if ( d_wallSec == t.d_wallSec ) {
		return d_wallNsec < t.d_wallNsec;
	}
	return d_wallSec < t.d_wallSec;
}

Duration Time::Sub(const Time & t) const {
	if ( d_monoID != 0 && d_monoID == t.d_monoID ) {
		// both have a monotonic timestamp issued from the same clock
		return int64_t(d_mono - t.d_mono);
	}

	int64_t seconds = d_wallSec - t.d_wallSec;
	int32_t nsecs = d_wallNsec - t.d_wallNsec;

	if ( seconds > MAX_SECOND_SINT64 ||
	     seconds <  MIN_SECOND_SINT64 ) {
		throw Overflow("duration");
	}

	seconds *= NANOS_PER_SECOND_SINT64;

	if ( (nsecs > 0 && seconds > MAX_SINT64 - nsecs) ||
	     (nsecs < 0 && seconds < MIN_SINT64 - nsecs) ) {
		throw Overflow("duration");
	}

	return seconds + nsecs;
}

bool Time::HasMono() const {
	return (d_monoID & HAS_MONO_BIT) != 0;
}

Time::MonoclockID Time::MonoID() const {
	if ( (d_monoID & HAS_MONO_BIT) == 0 ) {
		throw std::runtime_error("Time has no monotonic value");
	}
	return d_monoID & MONO_MASK;
}

uint64_t Time::MonotonicValue() const {
	if ( (d_monoID & HAS_MONO_BIT) == 0 ) {
		throw std::runtime_error("Time has no monotonic value");
	}
	return d_mono;
}

std::string Time::DebugString() const {
	std::ostringstream os;
	os << "{Time:" << *this;
	if ( HasMono() ) {
		os << ";monoID:" << MonoID()
		   << ";mono:" << MonotonicValue();
	}
	os << "}";
	return os.str();
}

Time::SortableKey Time::SortKey(const Time::ConstPtr & timePtr ) {
	return !timePtr ? std::make_pair(std::numeric_limits<int64_t>::min(),std::numeric_limits<int32_t>::min()) : timePtr->SortKey();
}

std::string Duration::ToString() const {
	std::ostringstream oss;
	oss << *this;
	return oss.str();
}



} // namespace artemis
} // namespace fort

std::ostream & operator<<(std::ostream & out,
                          const fort::artemis::Duration & d) {
	int64_t ns = d.Nanoseconds();
	if ( ns == 0 ) {
		return out << "0s";
	}
	std::string sign = "";

	if (ns == std::numeric_limits<int64_t>::min()) {
		return out << "-2562047h47m16.854775808s";
	}

	if (ns < 0) {
		sign = "-";
		ns = -ns;
	}

	if ( ns < fort::artemis::Duration::Microsecond.Nanoseconds() ) {
		return out << sign << ns << "ns";
	}

	if ( ns < fort::artemis::Duration::Millisecond.Nanoseconds() ) {
		return out << d.Microseconds() << "µs";
	}

	if ( ns < fort::artemis::Duration::Second.Nanoseconds() ) {
		return out << d.Milliseconds() << "ms";
	}

	int64_t minutes = ns / int64_t(60000000000LL);
	double seconds = fort::artemis::Duration(ns % 60000000000LL).Seconds();

	if ( minutes == 0 ) {
		return out << d.Seconds() << "s";
	}

	int64_t hours = minutes / 60;
	minutes = minutes % 60;
	auto flags = out.flags();
	out << std::setprecision(12);
	if ( hours == 0 ) {
		out << sign << minutes << "m" << seconds << "s";
		out.setf(flags);
		return out;
	}

	out << sign << hours << "h" << minutes << "m" << seconds << "s";
	out.setf(flags);

	return out;
}


std::ostream & operator<<(std::ostream & out,
                          const fort::artemis::Time & t) {
	return out << google::protobuf::util::TimeUtil::ToString(t.ToTimestamp());
}


std::ostream & operator<<(std::ostream & out, const fort::artemis::Time::ConstPtr & t ) {
	if (!t) {
		return out << "+/-∞";
	}
	return out << *t;
}
