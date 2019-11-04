#pragma once


#include <cstdint>

struct CameraConfiguration {
	CameraConfiguration()
		: FPS(20.0)
		, StrobeDuration(2000)
		, StrobeDelay(0)
		, Slave(false) {}

	double FPS;
	uint32_t StrobeDuration;
	int32_t  StrobeDelay;
	bool     Slave;
};
