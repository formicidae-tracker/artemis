#pragma once

#include <gtest/gtest.h>

#include "RingBuffer.h"

class RingBufferUTest : public ::testing::Test {
protected :
	typedef RingBuffer<int,4> RB;
	void SetUp();
	void TearDown();

	RB::Consumer::Ptr d_consumer;
	RB::Producer::Ptr d_producer;
};
