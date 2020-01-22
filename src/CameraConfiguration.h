#pragma once


#include <cstdint>

struct CameraConfiguration {
	CameraConfiguration()
		: FPS(20.0)
		, StrobeDuration(2000)
		, StrobeDelay(0)
		, Width(0)
		, Height(0) {}

	double FPS;
	uint32_t StrobeDuration;
	int32_t  StrobeDelay;
	int32_t Width;
	int32_t Height;
};
