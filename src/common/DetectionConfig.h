#pragma once

#include <string>
#include <cmath>





struct DetectionConfig {
	enum class FamilyType {
		TAG36H11 = 0,
	};
	static std::string FamilyName(FamilyType t);
	static FamilyType FamilyTypeFromName(const std::string & name);

	DetectionConfig()
		: Family(FamilyType::TAG36H11)
		, QuadDecimate(1.0)
		, QuadSigma(0.0)
		, RefineEdges(false)
		, QuadMinClusterPixel(5)
		, QuadMaxNMaxima(10)
		, QuadCriticalRadian(10*M_PI/180.0)
		, QuadMaxLineMSE(10.0)
		, QuadMinBWDiff(5) {
	}


	FamilyType  Family;
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
