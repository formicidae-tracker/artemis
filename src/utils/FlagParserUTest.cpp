/*
 * \file CliArgumentParserUTest.cpp
 *
 *  \date Oct 10, 2013
 *  \author Alexandre Tuleu
 */

#include "FlagParserUTest.hpp"
#include <memory>

#include <cmath>

#define EXPECT_NO_STD_THROW(statement) \
	EXPECT_NO_THROW({ \
			try { \
				statement \
			} catch ( const std::exception & e) { \
				ADD_FAILURE() << "Got unexpected error: " << e.what(); \
			} \
		})


class ArgHandler {
public :
	ArgHandler(const std::vector<std::string> argv)
		: d_argv(argv) {
	}

	~ArgHandler() {

	};
	int Argc() {
		return d_argv.size();
	}

	std::vector<char*> Argv() {
		std::vector<char*> res(d_argv.size() + 1, NULL);
		for( auto a = d_argv.cbegin(); a != d_argv.cend(); ++a) {

			size_t i = (size_t)std::distance(d_argv.cbegin(),a);
			res[i] = const_cast<char *> (a->c_str());
		}
		return res;
	}
private :
	std::vector<std::string> d_argv;
};

void FlagParserUTest::SetUp() {
	d_parser = std::make_shared<options::FlagParser>(options::FlagParser::Default,
	                                                 "test flag parser");
	d_options = std::make_shared<Options>();
	InitFlags();
}

void FlagParserUTest::InitFlags() {
	d_parser->AddFlag("string",d_options->StringFlag,"A string parameter",'s');
	d_parser->AddFlag("unsigned-int",d_options->UIntFlag,"An unsigned int parameter",'u');
	d_parser->AddFlag("int",d_options->UIntFlag,"An int parameter",'i');
	d_parser->AddFlag("flag", d_options->Flag, "A boolean flag", 'f');
}

void FlagParserUTest::TearDown() {
	d_parser = std::shared_ptr<options::FlagParser>();
	d_options = std::shared_ptr<Options>();
}

TEST_F(FlagParserUTest,CanParseUInt) {
	struct TestData {
		std::string AsString;
		uint32_t    AsUInt;
	};

	std::vector<TestData> data = {
		{ "123" , 123 },
		{ "0x123" , 0x123},
	};

	for ( auto d : data ) {
		ArgHandler h( { "foo" , "-u" , d.AsString });
		int argc = h.Argc();

		EXPECT_NO_STD_THROW({
				d_parser->Parse(argc,&(h.Argv()[0]));
			}) << "When parsing " << d.AsString;

		EXPECT_EQ(d.AsUInt,d_options->UIntFlag);
	}
}



TEST_F(FlagParserUTest,ExtractUsedParameter) {
	struct TestData {
		std::vector<std::string> Args;
		std::vector<std::string> Result;
	};

	std::vector<TestData> data = {
		{ {"foo"},{"foo"} },
		{ {"foo","bar", "baz"},{"foo","bar", "baz"} },
		{ {"foo", "--flag"},{"foo"} },
		{ {"foo", "--unsigned-int", "123", "foo"},{"foo", "foo"} },
		{ {"foo", "-fH", "123", "foo"},{"foo", "-H", "123", "foo"} },
	};

	// this parser will not modify the char **
	d_parser =  std::make_shared<options::FlagParser>(options::FlagParser::DoNotRemoveUsed |
	                                                  options::FlagParser::DoNotReportUnused, "foo");
	InitFlags();

	for ( auto d : data ) {
		ArgHandler h(d.Args);
		int argc = h.Argc();
		auto argv = h.Argv();
		EXPECT_NO_STD_THROW({d_parser->Parse(argc,&(argv[0]));});
		EXPECT_EQ(d.Args.size(),argc);

		for( size_t i = 0; i < std::min(d.Args.size(),(size_t)argc); ++i) {
			EXPECT_EQ(d.Args[i],argv[i]);
		}
	}

	d_parser =  std::make_shared<options::FlagParser>(options::FlagParser::DoNotReportUnused, "foo");
	InitFlags();

	for ( auto d : data ) {
		ArgHandler h(d.Args);
		int argc = h.Argc();
		auto argv = h.Argv();

		EXPECT_NO_STD_THROW({d_parser->Parse(argc,&(argv[0]));});
		EXPECT_EQ(d.Result.size(),argc);

		for( size_t i = 0; i < std::min(d.Result.size(),(size_t)argc); ++i) {
			EXPECT_EQ(d.Result[i],argv[i]);
		}
	}


}


TEST_F(FlagParserUTest,CanHaveMandatoryParameter) {
	std::string mandatory_flag;
	d_parser->AddFlag("mandatory-flag",mandatory_flag,"a flag that is mandatory",'m',true);

	EXPECT_NO_STD_THROW({
			ArgHandler h({"foo","-m","bar"});
			int argc = h.Argc();
			auto argv = h.Argv();
			d_parser->Parse(argc,&(argv[0]));
		});
	EXPECT_EQ("bar",mandatory_flag);

	EXPECT_NO_THROW({
			ArgHandler h({"foo","bar"});
			int argc = h.Argc();
			auto argv = h.Argv();
			try {
				d_parser->Parse(argc,&(argv[0]));
			} catch(const std::runtime_error & e) {
				EXPECT_EQ("Missing mandatory flag(s): {-m/--mandatory-flag}",std::string(e.what()));
			}
		});
}


TEST_F(FlagParserUTest,UsagePrinting) {
	std::ostringstream os;
	ASSERT_NO_THROW(d_parser->PrintUsage(os));
	EXPECT_EQ("test flag parser\n"
	          "option(s):\n"
	          "  -f/--flag         : A boolean flag\n"
	          "  -i/--int          : An int parameter (default: 0)\n"
	          "  -s/--string       : A string parameter (default: '')\n"
	          "  -u/--unsigned-int : An unsigned int parameter (default: 0)\n", os.str());
	os.str("");
	os.clear();
	d_parser->AppendUsage("foo [OPTIONS] file1 ... fileN");
	std::string mandatory("foo") ;
	d_parser->AddFlag("mandatory",mandatory,"A mandatory flag",'m',true);
	ASSERT_NO_THROW(d_parser->PrintUsage(os));
	EXPECT_EQ("test flag parser\n"
	          "usage(s):\n"
	          "  foo [OPTIONS] file1 ... fileN\n"
	          "\n"
	          "option(s):\n"
	          "  -f/--flag         : A boolean flag\n"
	          "  -i/--int          : An int parameter (default: 0)\n"
	          "  -m/--mandatory    : A mandatory flag (default: 'foo', mandatory)\n"
	          "  -s/--string       : A string parameter (default: '')\n"
	          "  -u/--unsigned-int : An unsigned int parameter (default: 0)\n", os.str());
}

TEST_F(FlagParserUTest,CanAddHelpFlag) {
	EXPECT_NO_STD_THROW({
			bool helpNeeded;
			d_parser->AddFlag<bool>("help",helpNeeded,"Prints this help and exit",'h');
		});
}
