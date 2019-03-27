/*
 * \file CliArgParser.imp.h
 *
 *  \date Oct 10, 2013
 *  \author Alexandre Tuleu
 */

#pragma once

#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <limits>


namespace options {

template <typename T>
inline FlagParser::ValueHolder<T>::ValueHolder(const std::string & name,
                                               const std::string & description,
                                               char shortName,
                                               T & value)
	: ValueHolderBase(name,description,shortName,true)
	, d_value(value){

}

// User should use the AddOption() function for boolean
template <>
inline FlagParser::ValueHolder<bool>::ValueHolder(const std::string & name,
                                                  const std::string & description,
                                                  char shortName,
                                                  bool & value)
	: ValueHolderBase(name,description,shortName,false)
	, d_value(value) {
	value = false;
}

template <typename T>
inline FlagParser::ValueHolder<T>::~ValueHolder(){
}

template <typename T>
inline void FlagParser::ValueHolder<T>::ParseArg(const char * arg) {
	std::istringstream iss(arg);
	iss.exceptions(std::istream::failbit|std::istream::badbit);
	iss >> std::setbase(0) >> d_value;
}

template <>
inline void FlagParser::ValueHolder<std::string>::ParseArg(const char * arg) {
	d_value = std::string(arg);
}

template <>
inline void FlagParser::ValueHolder<bool>::ParseArg(const char * ) {
	d_value = true;
}

template <typename T>
inline void FlagParser::ValueHolder<T>::PrintDefaultValue(std::ostream & out) const {
	out << d_value;
}

template <>
inline void FlagParser::ValueHolder<std::string>::PrintDefaultValue(std::ostream & out) const {
	out << "'" << d_value << "'";
}


template <typename T>
inline void FlagParser::AddFlag(const std::string & name,
                             T & value,
                             const std::string & description,
                             char shortName,
                             bool mandatory) {
	if ( std::is_same<T,bool>::value ) {
		mandatory = false;
	}

	CheckFlagName(name,shortName);

	ValueHolderBase::Ptr holder(new ValueHolder<T>(name,description,shortName,value));
	d_longFlags[name] = holder;
	if(shortName != NO_SHORT) {
		d_shortFlags[shortName] = holder;
	}

	if ( mandatory ) {
		d_mandatory.insert(holder);
	}

}


} /*namespace options */

