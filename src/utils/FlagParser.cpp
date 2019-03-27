#include "FlagParser.h"

#include <cstring>

#include <iomanip>

#include "StringManipulation.h"

using namespace options;

FlagParser::FlagParser(Mode m,
                       const std::string& description)
	: d_mode(m)
	, d_description(description) {
	if(d_description.empty()) {
		throw std::logic_error("FlagParser(): description is empty");
	}
}

void FlagParser::AppendUsage(const std::string& usage) {
	d_usages.push_back(usage);
}

void FlagParser::PrintUsage(std::ostream & out) const {

	out << d_description << std::endl;
	if(!d_usages.empty()) {
		out << "usage(s):" << std::endl;
		for(std::vector<std::string>::const_iterator u = d_usages.begin();
		    u != d_usages.end();
		    ++u) {
			out << "  " << *u << std::endl;
		}
		out << std::endl;
	}

	if(d_longFlags.empty()) {
		return;
	}
	out << "option(s):" << std::endl;
	size_t nameLength = 0;
	for( auto f = d_longFlags.cbegin(); f != d_longFlags.end(); ++f) {
		nameLength = std::max(nameLength,f->second->FullName().size());
	}


	for(auto f = d_longFlags.cbegin(); f != d_longFlags.end(); ++f) {
		out << "  "
		    << std::setfill(' ') << std::setw(nameLength) << std::left
		    << f->second->FullName()
		    << " : "
		    << f->second->Description();
		if (!f->second->NeedValue()) {
			out << std::endl;
			continue;
		}

		out << " (default: ";
		f->second->PrintDefaultValue(out);
		if ( d_mandatory.count(f->second) ){
			out << ", mandatory)";
		} else {
			out << ")";
		}
		out << std::endl;

	}
}

void FlagParser::Parse(int & argc,
                  char ** argv) {
	std::vector<std::string> args(argv, argv + argc);
	std::set<ValueHolderBase::Ptr> mandatoryFlags = d_mandatory;
	std::vector<int> toRemove;
	toRemove.reserve((size_t)argc);

	if (argc == 0) {
		throw std::runtime_error("Missing mandator first parameter");
	}
	for( auto a = args.cbegin() + 1; a != args.cend(); ++a) {
		if ( a->empty() ) {
			continue;
		}

		if ( a->at(0) != '-' ) {
			continue;
		}

		if ( base::HasPrefix(*a,"--") ) {
			//simple case, its a longname
			std::string longName= a->substr(2);
			auto fi = d_longFlags.find(longName);
			if ( fi == d_longFlags.end() ) {
				ReportUnused(*a);
				continue;
			}
			const char * value = "";
			if ( fi->second->NeedValue() ) {
				if ( a + 1 == args.cend() ) {
					throw std::runtime_error("Missing value for '" + fi->second->FullName() + "' flag");
				}
				toRemove.push_back((size_t)std::distance(args.cbegin(),a));
				++a;
				value = a->c_str();
			}
			try {
				fi->second->ParseArg(value);
			} catch (const std::exception & e) {
				throw std::runtime_error("'" + fi->second->FullName() + "' parse error: " + e.what());
			}

			toRemove.push_back((size_t)std::distance(args.cbegin(),a));
			mandatoryFlags.erase(fi->second);
			continue;
		}

		// ok we start by a '-', and its a list of flags, that should be either one with value

		if ( a->size() == 1 ) {
			throw std::runtime_error("Invalid flag '" + *a + "'");
		}
		std::string toKeep;
		for( auto f = a->cbegin() + 1; f != a->cend(); ++f) {
			if ( *f == 0) {
				break;
			}
			std::string flagName; flagName += *f;
			auto fi = d_shortFlags.find(*f);
			if ( fi == d_shortFlags.end() ) {
				ReportUnused(flagName);
				toKeep += *f;
				continue;
			}

			if (fi->second->NeedValue() && a->size() != 2) {
				throw std::runtime_error("Invalid value - flag  '" + flagName
				                         +  "' in multiple short flag '" + *a +  "'");
			}
			const char * value = "";
			if (fi->second->NeedValue() ) {
				if ( a + 1 == args.cend() ) {
					throw std::runtime_error("Missing value for '" + fi->second->FullName() + "' flag");
				}
				toRemove.push_back((size_t)std::distance(args.cbegin(),a));
				++a;
				value = a->c_str();
			}
			try {
				fi->second->ParseArg(value);
			} catch (const std::exception & e) {
				throw std::runtime_error("'" + fi->second->FullName() + "' parse error: " + e.what());
			}
			mandatoryFlags.erase(fi->second);
		}

		if ( !toKeep.empty() && ( (d_mode & DoNotRemoveUsed) == 0 ) ) {
			for ( auto f = toKeep.cbegin(); f != toKeep.cend(); ++f) {
				argv[(size_t)std::distance(args.cbegin(),a)][(size_t)std::distance(toKeep.cbegin(),f) + 1] = *f;
			}
			argv[(size_t)std::distance(args.cbegin(),a)][toKeep.size()+1] = 0;
		} else {
			toRemove.push_back((size_t)std::distance(args.cbegin(),a));
		}

	}

	if (! mandatoryFlags.empty() ) {
		std::vector<std::string> flagsName;
		flagsName.reserve(mandatoryFlags.size());
		for(auto f = mandatoryFlags.cbegin(); f != mandatoryFlags.cend(); ++f) {
			flagsName.push_back((*f)->FullName());
		}
		throw std::runtime_error("Missing mandatory flag(s): {" +
		                         base::JoinString(flagsName.cbegin(),flagsName.cend(),",") +
		                         "}");
	}

	if ( d_mode & DoNotRemoveUsed ) {
		return;
	}
	argc -= (int)toRemove.size();
	int skip = 0;
	auto nextSkip = toRemove.cbegin();
	for(int i = 0; i < argc; ++i) {
		while( nextSkip != toRemove.cend() && *nextSkip == (i + skip)) {
			++nextSkip;
			++skip;
		}
		if (skip == 0) {
			continue;
		}
		argv[i] = argv[i+skip];
	}
	argv[argc] = NULL;
}

FlagParser::ValueHolderBase::ValueHolderBase(const std::string & name,
                                             const std::string & description,
                                             char shortName,
                                             bool needValue)
	: d_name(name)
	, d_description(description)
	, d_shortName(shortName)
	, d_needValue(needValue) {
	base::TrimSpaces(d_description);
	if(description.empty()) {
		throw std::logic_error("Suitable description required : this option is not empty, isn't it ?");
	}

}

FlagParser::ValueHolderBase::~ValueHolderBase() {
}

const std::string & FlagParser::ValueHolderBase::Name() const {
	return d_name;
}

const std::string & FlagParser::ValueHolderBase::Description() const {
	return d_description;
}

std::string FlagParser::ValueHolderBase::FullName() const {
	if ( d_shortName != NO_SHORT ) {
		std::ostringstream os;
		os  << "-" << d_shortName << "/--" << d_name;
		return os.str();
	}
	return "--" + d_name;
}

bool FlagParser::ValueHolderBase::NeedValue() const {
	return d_needValue;
}

void FlagParser::CheckFlagName(const std::string & name,char shortName) const {
#define throw_bad_long_name() do {	  \
		throw std::logic_error("Invalid option name '" + name + "'"); \
	} while(0)

	//not using regex, too complicated.
	// must start by a alphabetic character and be non-empty
	if(name.size() < 2) {
		throw_bad_long_name();
	}

	if(!std::isalpha(name[0])) {
		throw_bad_long_name();
	}

	//now we check that amny following char are in [:word:\-]
	for(std::string::size_type i = 1;
	    i < name.size();
	    ++i) {
		if(std::isalnum(name[i]) || name[i] == '_'  || name[i] == '-') {
			continue;
		}
		throw_bad_long_name();
	}
	//this is the case, so long name  is checked
	#undef throw_bad_long_name

	if(shortName == 0) {
		return;
	}

	if( !(shortName >='a' && shortName <= 'z') &&
	    !(shortName >= 'A' && shortName <= 'Z') ) {
		std::ostringstream os;
		os << "invalid short option name '" << shortName << "'";
		throw std::logic_error(os.str());
	}

	ValuesByLong::const_iterator fi = d_longFlags.find(name);
	if (fi != d_longFlags.end()) {
		throw std::logic_error("option name '--" + name + "' is already in use");
	}

	if(shortName == 0) {
		return;
	}

	ValuesByShort::const_iterator fiC = d_shortFlags.find(shortName);
	if( fiC != d_shortFlags.end()) {
		std::ostringstream os;
		os << "Option short name '-" << shortName << "' is already in use";
		throw std::logic_error(os.str());
	}

}



void FlagParser::ReportUnused(const std::string & name) {
	if ( d_mode & DoNotReportUnused ) {
		return;
	}
	throw std::runtime_error("Unused flag '" + name + "'");
}


options::FlagParser::Mode operator | (options::FlagParser::Mode a,
                                      options::FlagParser::Mode b) {
	return (options::FlagParser::Mode)((int)a | (int)b);
}
