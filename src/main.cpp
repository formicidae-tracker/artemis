#include <glog/logging.h>

#include "Apriltag2Process.h"
#include "utils/FlagParser.h"




// DEFINE_string(apriltag_family,"36h11","The apriltag family to use");
// DEFINE_uint64(new_ant_roi_size,1000,"Upon a new ant detection size of the image to save");


struct Options {
	bool PrintHelp;

	AprilTag2Detector::Config AprilTag2;
};


void ParseArgs(int & argc, char ** argv,Options & opts ) {
	options::FlagParser parser(options::FlagParser::Default,"low-level vision detection for the FORmicidae Tracker");

	AprilTag2Detector::Config c;
	parser.AddFlag("help",opts.PrintHelp,"Print this help message",'h');

	parser.AddFlag("at-family",opts.AprilTag2.Family,"The apriltag2 family to use");
	parser.AddFlag("new-ant-roi-size", opts.AprilTag2.NewAntROISize, "Size of the image to save when a new ant is found");
	parser.AddFlag("at-quad-decimate",opts.AprilTag2.QuadDecimate,"Decimate original image for faster computation but worse pose estimation. Should be 1.0 (no decimation), 1.5, 2, 3 or 4");
	parser.AddFlag("at-quad-sigma",opts.AprilTag2.QuadSigma,"Apply a gaussian filter for quad detection, noisy image likes a slight filter like 0.8");
	parser.AddFlag("at-refine-edges",opts.AprilTag2.RefineEdges,"Refines the edge of the quad, especially needed if decimation is used, inexpensive");
	parser.AddFlag("at-refine-decode",opts.AprilTag2.RefineDecode,"Refines the tag code detection. Very useful for small tags");
	parser.AddFlag("at-refine-pose",opts.AprilTag2.RefinePose,"Refines the pose");
	parser.AddFlag("at-quad-min-cluster",opts.AprilTag2.QuadMinClusterPixel,"Minimum number of pixel to consider it a quad");
	parser.AddFlag("at-quad-max-n-maxima",opts.AprilTag2.QuadMaxNMaxima,"Number of candidate to consider to fit quad corner");
	parser.AddFlag("at-quad-critical-radian",opts.AprilTag2.QuadCriticalRadian,"Rejects quad with angle to close to 0 or 180 degrees");
	parser.AddFlag("at-quad-max-line-mse",opts.AprilTag2.QuadMaxLineMSE,"MSE threshold to reject a fitted quad");
	parser.AddFlag("at-quad-min-bw-diff",opts.AprilTag2.QuadMinBWDiff,"Difference in pixel value to consider a region black or white");
	parser.AddFlag("at-quad-deglitch",opts.AprilTag2.QuadDeglitch,"Deglitch only for noisy images");


	parser.Parse(argc,argv);
	if (opts.PrintHelp == true) {
		parser.PrintUsage(std::cerr);
		exit(0);
	}
}


void Execute(int argc, char ** argv) {
	::google::InitGoogleLogging(argv[0]);
	Options opts;
	ParseArgs(argc, argv,opts);
}

int main(int argc, char ** argv) {
	try {
		Execute(argc,argv);
	} catch (const std::exception & e) {
		LOG(FATAL) << "Got uncaught exception: " << e.what();
		return 1;
	}
}
