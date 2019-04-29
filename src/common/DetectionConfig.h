#pragma once

#include <string>
#include <cmath>

struct DetectionConfig {
	DetectionConfig()
		: Family("36h11")
		, QuadDecimate(1.0)
		, QuadSigma(0.0)
		, RefineEdges(false)
		, QuadMinClusterPixel(5)
		, QuadMaxNMaxima(10)
		, QuadCriticalRadian(10*M_PI/180.0)
		, QuadMaxLineMSE(10.0)
		, QuadMinBWDiff(5) {
	}


	std::string Family;
	float       QuadDecimate;
	float       QuadSigma;
	bool        RefineEdges;
	bool        NoRefineDecode;
	bool        RefinePose;
	int         QuadMinClusterPixel;
	int         QuadMaxNMaxima;
	float       QuadCriticalRadian;
	float       QuadMaxLineMSE;
	int         QuadMinBWDiff;
};
