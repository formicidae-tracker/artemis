#pragma once

#include <utils/Time.hpp>
#include <fort/tags/fort-tags.h>
#include <set>
#include <cstdint>

namespace options {
class FlagParser;
}

namespace fort {
namespace artemis {

struct GeneralOptions {
	GeneralOptions();

	void PopulateParser( options::FlagParser & parser);
 	void FinishParse();

	bool        PrintHelp;
	bool        PrintVersion;
	bool        PrintResolution;

	std::string LogDir;
	std::string StubImagePath;

	bool        TestMode;
	bool        LegacyMode;
};


struct DisplayOptions {

	std::vector<uint32_t> Highlighted;
	void PopulateParser( options::FlagParser & parser);
	void FinishParse();

private :
	std::string d_highlighted;
};

struct NetworkOptions {
	NetworkOptions();
	void PopulateParser( options::FlagParser & parser);
 	void FinishParse();


	std::string Host;
	std::string UUID;
	uint16_t    Port;
};

struct VideoOutputOptions {
	VideoOutputOptions();
	void PopulateParser( options::FlagParser & parser);
 	void FinishParse();


	size_t      Height;
	bool        AddHeader;
	bool        ToStdout;
};


struct ApriltagOptions {
	ApriltagOptions();
	void PopulateParser( options::FlagParser & parser);
 	void FinishParse();


	fort::tags::Family Family;
	float              QuadDecimate;
	float              QuadSigma;
	bool               RefineEdges;
	int                QuadMinClusterPixel;
	int                QuadMaxNMaxima;
	float              QuadCriticalRadian;
	float              QuadMaxLineMSE;
	int                QuadMinBWDiff;
	bool               QuadDeglitch;

private:
	std::string d_family;
};

struct CameraOptions {
	CameraOptions();
	void PopulateParser( options::FlagParser & parser);
 	void FinishParse();

	double    FPS;
	Duration  StrobeDuration;
	Duration  StrobeDelay;
	size_t    SlaveWidth;
	size_t    SlaveHeight;
private:
	std::string d_strobeDuration,d_strobeDelay;
};


struct ProcessOptions {
	ProcessOptions();
	void PopulateParser( options::FlagParser & parser);
 	void FinishParse();

	size_t             FrameStride;
	std::set<uint64_t> FrameID;

	std::string NewAntOutputDir;
	size_t      NewAntROISize;
	Duration    ImageRenewPeriod;
private:
	std::string d_imageRenewPeriod,d_frameIDs;
};

struct Options {
	GeneralOptions     General;
	DisplayOptions     Display;
	NetworkOptions     Network;
	VideoOutputOptions VideoOutput;
	ApriltagOptions    Apriltag;
	CameraOptions      Camera;
	ProcessOptions     Process;

	static Options Parse(int & argc, char ** argv, bool printHelp = false);

	void Validate();

private :
	void PopulateParser( options::FlagParser & parser);
	void FinishParse();
};

} // namespace artemis
} // namespace fort
