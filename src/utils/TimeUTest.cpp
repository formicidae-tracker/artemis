#include "TimeUTest.hpp"

#include "Time.hpp"

#include <google/protobuf/util/time_util.h>
#include <google/protobuf/util/message_differencer.h>

::testing::AssertionResult TimeEqual(const fort::artemis::Time & a,
                                     const fort::artemis::Time & b) {
	if ( a.Equals(b) == false ) {
		return ::testing::AssertionFailure() << "a: " << a.DebugString()
		                                     << "b: " << b.DebugString()
		                                     << " and a.Equals(b) returns false";
	}

	if ( google::protobuf::util::MessageDifferencer::Equals(a.ToTimestamp(),b.ToTimestamp()) == false ) {
		return ::testing::AssertionFailure() << "a: " << a.DebugString()
		                                     << "b: " << b.DebugString()
		                                     << " and a.Timestamp() and b.Timestamp() yield different results";
	}

	if ( a.HasMono() == false ) {
		if (b.HasMono() == true ) {
			return ::testing::AssertionFailure() << "a: " << a.DebugString()
			                                     << "b: " << b.DebugString()
			                                     << " a has no monotonic time but b has one";
		} else {
			return ::testing::AssertionSuccess();
		}
	}

	if ( b.HasMono() ==  false ) {
		return ::testing::AssertionFailure() << "a: " << a.DebugString()
		                                     << "b: " << b.DebugString()
		                                     << " a has monotonic time but b hasn't";
	}

	if ( a.MonoID() != b.MonoID() ) {
		return ::testing::AssertionFailure() << "a: " << a.DebugString()
		                                     << "b: " << b.DebugString()
		                                     << " a and b have different monotonic ID";
	}

	if ( a.MonotonicValue() != b.MonotonicValue() ) {
		return ::testing::AssertionFailure() << "a: " << a.DebugString()
		                                     << "b: " << b.DebugString()
		                                     << " a and b have different monotonic value";
	}

	return ::testing::AssertionSuccess();
}

::testing::AssertionResult TimePtrEqual(const fort::artemis::Time::ConstPtr & a,
                                         const fort::artemis::Time::ConstPtr & b) {
	if (!a && !b) {
		return ::testing::AssertionSuccess();
	}

	if ( (!a && b) || (!b && a) ) {
		return ::testing::AssertionFailure() << "a: " << a << " != b: " << b;
	}
	return TimeEqual(*a,*b);
}



namespace fort {
namespace artemis {

TEST_F(TimeUTest,DurationCast) {
	Duration b(std::chrono::hours(1));

	EXPECT_EQ(b.Hours(),1.0);
	EXPECT_EQ(b.Minutes(),60.0);
	EXPECT_EQ(b.Seconds(),3600.0);
	EXPECT_EQ(b.Milliseconds(),3.6e6);
	EXPECT_EQ(b.Microseconds(),3.6e9);
	EXPECT_EQ(b.Nanoseconds(),3.6e12);

}


TEST_F(TimeUTest,DurationParsing) {
	//dataset taken from golang sources

	struct TestData {
		std::string Input;
		bool OK;
		Duration Expected;
	};

	std::vector<TestData> data
		= {
		   {"0",true,0},
		   {"0s",true,0},
		   {"5s", true, 5 * Duration::Second},
		   {"30s", true, 30 * Duration::Second},
		   {"1478s", true, 1478 * Duration::Second},
		   // sign
		   {"-5s", true, -5 * Duration::Second},
		   {"+5s", true, 5 * Duration::Second},
		   {"-0", true, 0},
		   {"+0", true, 0},
		   // decimal
		   {"5.0s", true, 5 * Duration::Second},
		   {"5.6s", true, 5*Duration::Second + 600*Duration::Millisecond},
		   {"5.s", true, 5 * Duration::Second},
		   {".5s", true, 500 * Duration::Millisecond},
		   {"1.0s", true, 1 * Duration::Second},
		   {"1.00s", true, 1 * Duration::Second},
		   {"1.004s", true, 1*Duration::Second + 4*Duration::Millisecond},
		   {"1.0040s", true, 1*Duration::Second + 4*Duration::Millisecond},
		   {"100.00100s", true, 100*Duration::Second + 1*Duration::Millisecond},
		   // different units
		   {"10ns", true, 10 * Duration::Nanosecond},
		   {"11us", true, 11 * Duration::Microsecond},
		   {"12µs", true, 12 * Duration::Microsecond}, // U+00B5
		   {"12μs", true, 12 * Duration::Microsecond}, // U+03BC
		   {"13ms", true, 13 * Duration::Millisecond},
		   {"14s", true, 14 * Duration::Second},
		   {"15m", true, 15 * Duration::Minute},
		   {"16h", true, 16 * Duration::Hour},
		   {"16h0m0s", true, 16 * Duration::Hour},
		   // composite durations
		   {"3h30m", true, 3*Duration::Hour + 30*Duration::Minute},
		   {"10.5s4m", true, 4*Duration::Minute + 10*Duration::Second + 500*Duration::Millisecond},
		   {"-2m3.4s", true, -(2*Duration::Minute + 3*Duration::Second + 400*Duration::Millisecond)},
		   {"1h2m3s4ms5us6ns", true, 1*Duration::Hour + 2*Duration::Minute + 3*Duration::Second + 4*Duration::Millisecond + 5*Duration::Microsecond + 6*Duration::Nanosecond},
		   {"39h9m14.425s", true, 39*Duration::Hour + 9*Duration::Minute + 14*Duration::Second + 425*Duration::Millisecond},
		   // large value
		   {"52763797000ns", true, 52763797000 * Duration::Nanosecond},
		   // more than 9 digits after decimal point, see https://golang.org/issue/6617
		   {"0.3333333333333333333h", true, 20 * Duration::Minute},
		   // 9007199254740993 = 1<<53+1 cannot be stored precisely in a float64
		   {"9007199254740993ns", true, ( (int64_t(1)<<53) + int64_t(1) ) * Duration::Nanosecond },
		   // largest duration that can be represented by int64 in nanoseconds
		   {"9223372036854775807ns", true, std::numeric_limits<int64_t>::max() },
		   {"9223372036854775.807us", true, std::numeric_limits<int64_t>::max() },
		   {"9223372036s854ms775us807ns", true, std::numeric_limits<int64_t>::max() },
		   // large negative value
		   {"-9223372036854775807ns", true, Duration(-1L<<63) + 1*Duration::Nanosecond},
		   // huge string; issue 15011.
		   {"0.100000000000000000000h", true, 6 * Duration::Minute},
		   // This value tests the first overflow check in leadingFraction.
		   {"0.830103483285477580700h", true, 49*Duration::Minute + 48*Duration::Second + 372539827*Duration::Nanosecond},

		   	// errors
		   	{"", false, 0},
		   	{"3", false, 0},
		   	{"-", false, 0},
		   	{"s", false, 0},
		   	{".", false, 0},
		   	{"-.", false, 0},
		   	{".s", false, 0},
		   	{"+.s", false, 0},
		   	{"3000000h", false, 0},                  // overflow
		   	{"9223372036854775808ns", false, 0},     // overflow
		   	{"9223372036854775.808us", false, 0},    // overflow
		   	{"9223372036854ms775us808ns", false, 0}, // overflow
		   	// largest negative value of type int64 in nanoseconds should fail
		   	// see https://go-review.googlesource.com/#/c/2461/
		   	{"-9223372036854775808ns", false, 0},
	};

	for ( const auto & d : data) {

		if ( d.OK == true ) {
			try {
				auto res = Duration::Parse(d.Input);
				EXPECT_EQ(res,d.Expected) << "parsing '" << d.Input << "'";
			} catch (const std::exception & e ) {
				ADD_FAILURE() << "Unexpected exception while parsing '" << d.Input
				              << "': " << e.what();
			}
		} else {
			try {
				auto res = Duration::Parse(d.Input);
				ADD_FAILURE() << "Should have received an exception while parsing '" << d.Input << "' but got result: " << res.Nanoseconds();
			} catch ( const std::exception & e) {

			}
		}
	}
}


TEST_F(TimeUTest,DurationFormatting) {
	struct TestData {
		std::string Expected;
		Duration Value;
	};

	std::vector<TestData> data
		= {
		   {"0s", 0},
		   {"1ns", 1 * Duration::Nanosecond},
		   {"1.1µs", 1100 * Duration::Nanosecond},
		   {"2.2ms", 2200 * Duration::Microsecond},
		   {"3.3s", 3300 * Duration::Millisecond},
		   {"-4.4s", -4400 * Duration::Millisecond},
		   {"4m5s", 4*Duration::Minute + 5*Duration::Second},
		   {"4m5.001s", 4*Duration::Minute + 5001*Duration::Millisecond},
		   {"5h6m7.001s", 5*Duration::Hour + 6*Duration::Minute + 7001*Duration::Millisecond},
		   {"8m1e-09s", 8*Duration::Minute + 1*Duration::Nanosecond},
		   {"2562047h47m16.854775807s", std::numeric_limits<int64_t>::max()},
		   {"-2562047h47m16.854775808s", std::numeric_limits<int64_t>::min()},
	};

	for ( const auto & d : data ) {
		std::ostringstream os;
		os << d.Value;
		EXPECT_EQ(os.str(),d.Expected);
	}
}


TEST_F(TimeUTest,HasMonotonicClock) {

	struct TestData {
		Time T;
		bool OK;
		Time::MonoclockID Expected;
	};
	timeval tv;
	//avoid overflow because using unitialized data.
	tv.tv_sec = 1000;
	tv.tv_usec = 10;

	google::protobuf::Timestamp pb;
	std::vector<TestData> data
		= {
		   { Time(), false , 0},
		   { Time::Now(), true , Time::SYSTEM_MONOTONIC_CLOCK},
		   { Time::FromTimeT(10), false , 0},
		   { Time::FromTimeval(tv), false , 0},
		   { Time::FromTimestamp(pb), false , 0},
		   { Time::FromTimestampAndMonotonic(pb,0,1), true, 1 },
		   { Time::Now().Add(2 * Duration::Nanosecond), true, Time::SYSTEM_MONOTONIC_CLOCK},
	};

	for ( const auto & d : data) {
		EXPECT_EQ(d.T.HasMono(),d.OK);
		if ( d.OK == true ) {
			EXPECT_NO_THROW({
					EXPECT_EQ(d.T.MonoID(),d.Expected);
				});
		} else {
			EXPECT_THROW({
					d.T.MonoID();
				},std::exception);
		}
	}

}

TEST_F(TimeUTest,TimeSubstraction) {

	google::protobuf::Timestamp zero = google::protobuf::util::TimeUtil::TimeTToTimestamp(0);
	google::protobuf::Timestamp oneSecond = google::protobuf::util::TimeUtil::TimeTToTimestamp(1);
	google::protobuf::Timestamp twoSecond = google::protobuf::util::TimeUtil::TimeTToTimestamp(2);

	struct TestData {
		Time A;
		Time B;
		Duration Expected;
		char Comp;
	};

	std::vector<TestData> data
		= {
		   {
		    Time::FromTimestamp(zero),
		    Time::FromTimestamp(zero),
		    0,
		    '=',
		   },
		   {
		    Time::FromTimestamp(oneSecond),
		    Time::FromTimestamp(zero),
		    1 * Duration::Second,
		    '>',
		   },
		   {
		    Time::FromTimestamp(oneSecond),
		    Time::FromTimestamp(twoSecond),
		    -1 * Duration::Second,
		    '<',
		   },
		   {
		    Time::FromTimestampAndMonotonic(twoSecond,(2*Duration::Second + 1).Nanoseconds(),1),
		    Time::FromTimestamp(oneSecond),
		    1 * Duration::Second,
		    '>',
		   },
		   {
		    Time::FromTimestamp(twoSecond),
		    Time::FromTimestampAndMonotonic(oneSecond,(1*Duration::Second - 1).Nanoseconds(),1),
		    1 * Duration::Second,
		    '>',
		   },
		   {
		    Time::FromTimestampAndMonotonic(twoSecond, (2*Duration::Second + 1).Nanoseconds(),1),
		    Time::FromTimestampAndMonotonic(oneSecond, (1*Duration::Second - 1).Nanoseconds(),1),
		    1 * Duration::Second + 2 * Duration::Nanosecond,
		    '>',
		   },
		   {
		    //won't use monotonic clock as ID don't matches
		    Time::FromTimestampAndMonotonic(twoSecond,(2*Duration::Second + 1).Nanoseconds(),1),
		    Time::FromTimestampAndMonotonic(oneSecond,(1*Duration::Second - 1).Nanoseconds(),2),
		    1 * Duration::Second,
		    '>',
		   },
		   {
		    Time::FromTimestamp(zero),
		    Time::FromTimestamp(zero).Add(-1*Duration::Nanosecond),
		    1 * Duration::Nanosecond,
		    '>',
		   },

	};


	for( const auto & d : data ) {
		EXPECT_EQ(d.A.Sub(d.B),d.Expected);
		switch(d.Comp) {
		case '=' :
			EXPECT_TRUE(d.A.Equals(d.B));
			break;
		case '<':
			EXPECT_TRUE(d.A.Before(d.B));
			break;
		case '>':
			EXPECT_TRUE(d.A.After(d.B));
			break;
		default:
			ADD_FAILURE() << "Intern test failure: unknown comparison test '" << d.Comp << "'";
		}
	}


}


TEST_F(TimeUTest,Overflow) {

	EXPECT_THROW({
			google::protobuf::Timestamp a;
			a.set_seconds(std::numeric_limits<int64_t>::max());
			a.set_nanos((1*Duration::Second + 1).Nanoseconds());
			auto t = Time::FromTimestamp(a);
		}, Time::Overflow);

	EXPECT_THROW({
			google::protobuf::Timestamp a;
			a.set_seconds(std::numeric_limits<int64_t>::min());
			a.set_nanos(-1);
			auto t = Time::FromTimestamp(a);
		}, Time::Overflow);

	EXPECT_THROW({
			google::protobuf::Timestamp a;
			auto t = Time::FromTimestampAndMonotonic(a,std::numeric_limits<uint64_t>::max(),1);
			t.Add(1);
		}, Time::Overflow);

	EXPECT_THROW({
			google::protobuf::Timestamp a;
			auto t = Time::FromTimestampAndMonotonic(a,1,1);
			t.Add(-2);
		}, Time::Overflow);


	google::protobuf::Timestamp a,b;
	a.set_seconds(std::numeric_limits<int64_t>::max()/1e9 + 1);
	b.set_seconds(std::numeric_limits<int64_t>::min()/1e9 - 1);

	EXPECT_THROW({
			Time::FromTimestamp(a).Sub(Time::FromTimestamp(b));
		}, Time::Overflow);

	EXPECT_THROW({
			Time::FromTimestamp(b).Sub(Time::FromTimestamp(a));
		}, Time::Overflow);


	a.set_seconds(std::numeric_limits<int64_t>::max()/1e9);
	b.set_seconds(0);
	EXPECT_NO_THROW({
			Time::FromTimestamp(a).Sub(Time::FromTimestamp(b));
		});
	a.set_nanos( std::numeric_limits<int32_t>::max() );
	EXPECT_THROW({
			Time::FromTimestamp(a).Sub(Time::FromTimestamp(b));
		},Time::Overflow);

	a.set_nanos(1e9-1);
	b.set_seconds(0);
	b.set_nanos(0);
	EXPECT_THROW({
			Time::FromTimestamp(b).Sub(Time::FromTimestamp(a));
		},Time::Overflow);




	EXPECT_THROW({
			Time::MonoFromSecNSec(std::numeric_limits<uint64_t>::max(), 0);
		},Time::Overflow);

	EXPECT_THROW({
			Time::MonoFromSecNSec(std::numeric_limits<uint64_t>::max()/1e9, 1e9);
		},Time::Overflow);

	EXPECT_THROW({
			Time::FromTimestampAndMonotonic(a,
			                                0,
			                                uint32_t(std::numeric_limits<int32_t>::max()) + uint32_t(1));
		}, Time::Overflow);

}


TEST_F(TimeUTest,TimeConversion) {

	time_t t = 10;
	EXPECT_EQ(Time::FromTimeT(t).ToTimeT(),t);

	timeval tv;
	tv.tv_sec = 11;
	tv.tv_usec = 10002;
	auto res = Time::FromTimeval(tv).ToTimeval();
	EXPECT_EQ(res.tv_sec,tv.tv_sec);
	EXPECT_EQ(res.tv_usec,tv.tv_usec);

	google::protobuf::Timestamp pb,resC,resInPlace;

	pb.set_seconds(-2);
	pb.set_nanos(3);

	resC = Time::FromTimestamp(pb).ToTimestamp();
	Time::FromTimestamp(pb).ToTimestamp(&resInPlace);

	EXPECT_EQ(resC,pb);
	EXPECT_EQ(resInPlace,pb);


}


TEST_F(TimeUTest,TimeFormat) {
	struct TestData {
		int64_t Sec;
		int64_t Nanos;
		std::string Expected;
		std::string Input;
	};

	std::vector<TestData> data
		= {
		   { ((2*365*24+5)*Duration::Hour + 20 * Duration::Second).Nanoseconds()/1000000000LL,
		     (21 * Duration::Millisecond).Nanoseconds(),
		     "1972-01-01T05:00:20.021Z",
		     "1972-01-01T10:00:20.021+05:00"},
	};


	for ( const auto & d : data ) {
		google::protobuf::Timestamp pb;
		pb.set_seconds(d.Sec);
		pb.set_nanos(d.Nanos);
		auto expectedTime = Time::FromTimestamp(pb);
		std::ostringstream os;
		os << expectedTime;
		EXPECT_EQ(os.str(),d.Expected);
		auto parsed = Time::Parse(d.Input);
		EXPECT_TRUE(parsed.Equals(expectedTime));
	}

}


TEST_F(TimeUTest,Rounding) {
	struct TestData {
		Time Value;
		Duration Round;
		Time Expected;
	};

	std::vector<TestData> testdata =
		{
		 {
		  Time::Parse("2020-03-20T15:34:08.865123567Z"),
		  10 * Duration::Nanosecond,
		  Time::Parse("2020-03-20T15:34:08.86512357Z"),
		 },
		 {
		  Time::Parse("2020-03-20T15:34:08.865123567Z"),
		  100 * Duration::Nanosecond,
		  Time::Parse("2020-03-20T15:34:08.8651236Z"),
		 },
		 {
		  Time::Parse("2020-03-20T15:34:08.865123567Z"),
		  1 * Duration::Microsecond,
		  Time::Parse("2020-03-20T15:34:08.865124Z"),
		 },
		 {
		  Time::Parse("2020-03-20T15:34:08.865123567Z"),
		  10 * Duration::Microsecond,
		  Time::Parse("2020-03-20T15:34:08.86512Z"),
		 },
		 {
		  Time::Parse("2020-03-20T15:34:08.865123567Z"),
		  100 * Duration::Microsecond,
		  Time::Parse("2020-03-20T15:34:08.8651Z"),
		 },
		 {
		  Time::Parse("2020-03-20T15:34:08.865123567Z"),
		  1 * Duration::Millisecond,
		  Time::Parse("2020-03-20T15:34:08.865Z"),
		 },
		 {
		  Time::Parse("2020-03-20T15:34:08.865123567Z"),
		  10 * Duration::Millisecond,
		  Time::Parse("2020-03-20T15:34:08.87Z"),
		 },
		 {
		  Time::Parse("2020-03-20T15:34:08.865123567Z"),
		  100 * Duration::Millisecond,
		  Time::Parse("2020-03-20T15:34:08.9Z"),
		 },
		 {
		  Time::Parse("2020-03-20T15:34:08.865123567Z"),
		  Duration::Second,
		  Time::Parse("2020-03-20T15:34:09Z"),
		 },
		 {
		  Time::Parse("2020-03-20T15:34:08.865123567Z"),
		  2*Duration::Second,
		  Time::Parse("2020-03-20T15:34:08Z"),
		 },
		 {
		  Time::Parse("2020-03-20T15:34:08.865123567Z"),
		  Duration::Minute,
		  Time::Parse("2020-03-20T15:34:00Z"),
		 },
		 {
		  Time::Parse("2020-03-20T15:34:08.865123567Z"),
		  Duration::Hour,
		  Time::Parse("2020-03-20T16:00:00Z"),
		 },
		 {
		  Time::Parse("2020-03-20T15:34:08.865123567Z"),
		  24 *Duration::Hour,
		  Time::Parse("2020-03-21T00:00:00Z"),
		 },
		};

	for ( const auto & d : testdata ) {
		EXPECT_TRUE(TimeEqual(d.Value.Round(d.Round),d.Expected));
	}

	std::vector<Duration> faildata =
		{
		 1 * Duration::Second + 1 * Duration::Millisecond,
		 2 * Duration::Millisecond,
		};

	for ( const auto & f : faildata ) {
		EXPECT_THROW(Time().Round(f),std::runtime_error);
	}

	auto now = Time::Now();
	ASSERT_TRUE(now.HasMono());
	EXPECT_FALSE(now.Round(Duration::Nanosecond).HasMono());

}

} // namespace myrmidon
} // namespace fort
