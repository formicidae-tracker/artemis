/*
 * \file CliArgParser.h
 *
 *  \date Oct 10, 2013
 *  \author Alexandre Tuleu
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <set>

namespace options {
// program argument flag parser
//
// <FlagParser> can be used to define command line flag that are
// extracted from the program argument (`argc`and  `argv`).
//
// User can define flags of the form '-x/--long-name' to be extracted
// and parsed to a value using <AddFlag>. The parser supports :
// * short and long name for a flag
// * defining multiple short flag in one argument, i.e. '-xFv'
// * specification of mandatory flag
//
// The method <Parse> can be used to extract those flag from `argc`
// and `argv`. Depending on the <Mode> of the parser, those will be
// modified by removing the extracted flags.
class FlagParser {
public:
	enum Mode {
		// Use all default parser parameter
		Default           = 0,
		// Do not throw exception if an unknown flag is found
		DoNotReportUnused = (1 << 0),
		// Do not modify `argc`/`argv` in <Parse>
		DoNotRemoveUsed   = (1 << 1),
	};

	// Constant that unspecifies
	static const char NO_SHORT = 0;


	// Creates a new <FlagParser>
	// @m the <Mode> of the parser
	// @description a description that would be displayed by <PrintUsage>.
	FlagParser(Mode m,
	           const std::string & description);

	// Appends an usage
	// @usage an usage to append
	//
	// Appends an usage in the list of usages that would be displayed
	// by <PrintUsage>
	void AppendUsage(const std::string & usage);

	// Adds a new flag
	// @T the type of variable, should be parsable by std::istream
	// @name the name of the flag
	// @value a reference for extracting the value
	// @description description of the flag
	// @shortName a one character short name for the flag
	// @mandatory specifies if the flag is mandatory
	template<typename T> void AddFlag(const std::string & name,
	                                  T & value,
	                                  const std::string & description,
	                                  char shortName = NO_SHORT,
	                                  bool mandatory = false);

	// Parses the program arguments
	// @argc the number of arguments
	// @argv the NULL terminated list of argument
	//
	// Parses the command line arguments and extract the flags from
	// it. By default the parser will modify <argv> and remove from it
	// the extracted flag and their values.
	void Parse(int & argc,
	           char ** argv);

	// Prinst a nice help usage
	// @out the <std::ostream> to format to
	void PrintUsage(std::ostream & out) const;

private :
	class ValueHolderBase {
	public :
		ValueHolderBase(const std::string & longName,
		                const std::string & description,
		                char shortName,
		                bool needValue);
		virtual ~ValueHolderBase();
		typedef std::shared_ptr<ValueHolderBase> Ptr;
		virtual void ParseArg(const char * arg) = 0;
		virtual void PrintDefaultValue(std::ostream & out) const = 0;

		const std::string & Name() const;
		const std::string & Description() const;
		std::string FullName() const;

		bool NeedValue() const;
	private :
		std::string d_name;
		std::string d_description;
		char d_shortName;
		bool d_needValue;
	};



	template <typename T>
	class ValueHolder : public ValueHolderBase {
	public :
		ValueHolder(const std::string & name,
                    const std::string & description,
                    char shortName,
                    T & value);
		virtual ~ValueHolder();
		void ParseArg(const char * arg);
		void PrintDefaultValue(std::ostream & out) const;
	private :
		T & d_value;
	};

	void CheckFlagName(const std::string & longName, char shortName) const;

	void ReportUnused(const std::string & name);

	typedef std::map<char,ValueHolderBase::Ptr> ValuesByShort;
	typedef std::map<std::string,ValueHolderBase::Ptr> ValuesByLong;

	Mode                     d_mode;
	std::string              d_description;
	std::vector<std::string> d_usages;

	ValuesByLong  d_longFlags;
	ValuesByShort d_shortFlags;
	std::set<ValueHolderBase::Ptr> d_mandatory;
};

} /* namespace options */

options::FlagParser::Mode operator | (options::FlagParser::Mode a,
                                      options::FlagParser::Mode b);

#include "FlagParserImpl.h"
