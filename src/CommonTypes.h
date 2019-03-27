#pragma once

#include "RingBuffer.h"

#include <sstream>

typedef RingBuffer<std::stringstream,16> SerializedMessageBuffer;


struct NewAntDescription {
	int32_t ID;
	cv::Mat Picture;
};

typedef RingBuffer<NewAntDescription,256> UnknownAntsBuffer;
