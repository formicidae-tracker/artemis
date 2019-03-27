/*
 * \file CliArgumentParserUTest.h
 *
 *  \date Oct 10, 2013
 *  \author Alexandre Tuleu
 */

#ifndef BIOROB_CPP_CLIARGUMENTPARSERUTEST_H_
#define BIOROB_CPP_CLIARGUMENTPARSERUTEST_H_


#include <gtest/gtest.h>

#include "FlagParser.h"
#include <memory>

class FlagParserUTest : public ::testing::Test {
protected :
	void SetUp();
	void TearDown();

	std::shared_ptr<options::FlagParser> d_parser;
	struct Options {
		std::string StringFlag;
		uint32_t    UIntFlag;
		int32_t     IntFlag;
		bool        Flag;
		Options() : UIntFlag(0), IntFlag(0), Flag(0){};
	};

	void InitFlags();
	std::shared_ptr<Options> d_options;
};


#endif /* BIOROB_CPP_CLIARGUMENTPARSERUTEST_H_ */
