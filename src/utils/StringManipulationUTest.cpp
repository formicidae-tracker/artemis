#include "StringManipulation.hpp"
#include "StringManipulationUTest.hpp"

using namespace base;

TEST_F(StringUTest,TrimLeftFunc) {
	struct TestData {
		std::string                 Base;
		std::function<bool (char )> Predicate;
		std::string                 Result;
	};

	std::vector<TestData> data = {
		{"foo",[](char c) { return c == 'o';},"foo"},
		{"foof",[](char c) { return c == 'f';},"oof"},
		{"ofoo",[](char c) { return c == 'o';},"foo"},
	};

	for ( auto d : data) {
		std::string s(d.Base);
		EXPECT_EQ(d.Result,TrimLeftFunc(s,d.Predicate));
	}
}

TEST_F(StringUTest,TrimRightFunc) {
	struct TestData {
		std::string                 Base;
		std::function<bool (char )> Predicate;
		std::string                 Result;
	};

	std::vector<TestData> data = {
		{"foo",[](char c) { return c == 'o';},"f"},
		{"foof",[](char c) { return c == 'f';},"foo"},
		{"ofoo",[](char c) { return c == 'o';},"of"},
	};

	for ( auto d : data) {
		std::string s(d.Base);
		EXPECT_EQ(d.Result,TrimRightFunc(s,d.Predicate));
	}
}



TEST_F(StringUTest,TrimFunc) {
	struct TestData {
		std::string                 Base;
		std::function<bool (char )> Predicate;
		std::string                 Result;
	};

	std::vector<TestData> data = {
		{"foo",[](char c) { return c == 'o';},"f"},
		{"foof",[](char c) { return c == 'f';},"oo"},
		{"ofoo",[](char c) { return c == 'o';},"f"},
		{"ofoofo",[](char c) { return c == 'o';},"foof"},
	};

	for ( auto d : data) {
		std::string s(d.Base);
		EXPECT_EQ(d.Result,TrimFunc(s,d.Predicate));
	}
}


TEST_F(StringUTest,TrimLeftCutset) {
	struct TestData {
		std::string Base;
		std::string Cutset;
		std::string Result;
	};

	std::vector<TestData> data = {
		{"foo" , "abc" , "foo"},
		{"acbfoo", "ab" , "cbfoo"},
		{"cabfoo", "abc" , "foo"},
		{"cabfooabc", "abc" , "fooabc"}
	};

	for (auto d : data ) {
		std::string s(d.Base);
		EXPECT_EQ(d.Result,TrimLeftCutset(s,d.Cutset));
	}
}


TEST_F(StringUTest,TrimRightCutset) {
	struct TestData {
		std::string Base;
		std::string Cutset;
		std::string Result;
	};

	std::vector<TestData> data = {
		{"foo" , "abc" , "foo"},
		{"acbfoo", "ab" , "acbfoo"},
		{"cabfoo", "abc" , "cabfoo"},
		{"cabfooabc", "abc" , "cabfoo"},
		{"fooacb", "ab" , "fooac"},
		{"foocab", "abc" , "foo"}
	};

	for (auto d : data ) {
		std::string s(d.Base);
		EXPECT_EQ(d.Result,TrimRightCutset(s,d.Cutset));
	}
}

TEST_F(StringUTest,TrimCutset) {
	struct TestData {
		std::string Base;
		std::string Cutset;
		std::string Result;
	};

	std::vector<TestData> data = {
		{"foo" , "abc" , "foo"},
		{"acbfoobca", "ab" , "cbfoobc"},
		{"cabfoo", "abc" , "foo"},
		{"cabfooabc", "abc" , "foo"},
		{"fooacb", "ab" , "fooac"},
		{"foocab", "abc" , "foo"}
	};

	for (auto d : data ) {
		std::string s(d.Base);
		EXPECT_EQ(d.Result,TrimCutset(s,d.Cutset));
	}
}


TEST_F(StringUTest,TrimPrefix) {
	struct TestData {
		std::string Base;
		bool        HasPrefix;
		std::string Prefix;
		std::string Result;
	};

	std::vector<TestData> data = {
		{"fooabc",false,"abc","fooabc"},
		{"abcfoo",true,"abc","foo"},
		{"acbfoo",false,"abc","acbfoo"},
		{"ab",false,"abc","ab"},
	};

	for( auto d : data) {
		std::string s(d.Base);
		EXPECT_EQ(d.HasPrefix,HasPrefix(d.Base,d.Prefix));
		EXPECT_EQ(d.Result,TrimPrefix(s,d.Prefix));
	}

}


TEST_F(StringUTest,TrimSuffix) {
	struct TestData {
		std::string Base;
		bool        HasSuffix;
		std::string Suffix;
		std::string Result;
	};

	std::vector<TestData> data = {
		{"fooabc",true,"abc","foo"},
		{"abcfoo",false,"abc","abcfoo"},
		{"fooacb",false,"abc","fooacb"},
		{"bc",false,"abc","bc"},
	};

	for( auto d : data) {
		std::string s(d.Base);
		EXPECT_EQ(d.HasSuffix,HasSuffix(d.Base,d.Suffix));
		EXPECT_EQ(d.Result,TrimSuffix(s,d.Suffix));
	}

}


TEST_F(StringUTest,TrimSpaces) {
	struct TestData {
		std::string Base;
		std::string Result;
	};

	std::vector<TestData> data = {
		{" \t\n", ""},
		{" Hello World!\n","Hello World!"},
	};

	for ( auto d : data) {
		std::string s(d.Base);
		EXPECT_EQ(d.Result,TrimSpaces(s));
	}
}

TEST_F(StringUTest,SplitString) {
	// The following test data is extracted from golang sources

	std::string abcd = "abcd";
	std::string faces = u8"☺☻☹";
	std::string commas = "1,2,3,4";
	std::string dots = "1....2....3....4";

	struct TestData {
		std::string Base;
		std::string Sep;
		std::vector<std::string> Result;
	};

	std::vector<TestData> data = {
		{abcd, "a", {"", "bcd"}},
		{abcd, "z", {"abcd"}},
		{abcd, "", {"a", "b", "c", "d"}},
		{commas, ",", {"1", "2", "3", "4"}},
		{dots, "...", {"1", ".2", ".3", ".4"}},
		{faces, "☹", {"☺☻", ""}},
		{faces, "~", {faces}},
		{faces, "", {"☺", "☻", "☹"}},
		{"1 2 3 4", " ", {"1", "2", "3", "4"}},
		{"1 2", " ", {"1", "2"}},
	};

	for( auto d : data ) {
		std::vector<std::string> splitted;
		splitted.reserve(d.Result.size());
		base::SplitString(d.Base.cbegin(),d.Base.cend(),d.Sep,
		                  std::back_inserter<std::vector<std::string>>(splitted));
		EXPECT_EQ(d.Result.size(),splitted.size()) << "When splitting '" << d.Base
		                                           << "' with '" << d.Sep << "'";
		for( size_t i = 0; i < std::min(d.Result.size(),splitted.size()); ++i) {
			EXPECT_EQ(d.Result[i],splitted[i]) << "On chunk " << i
			                                   << " when splitting '" << d.Base
			                                   << "' with '" << d.Sep << "'";
		}
	}

}
