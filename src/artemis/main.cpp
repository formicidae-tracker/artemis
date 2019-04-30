#include <glog/logging.h>

#include <thread>

#include "ResizeProcess.h"
#include "OutputProcess.h"
#include "AntCataloguerProcess.h"
#include "DrawDetectionProcess.h"
#include "utils/FlagParser.h"
#include "utils/StringManipulation.h"
#include "EuresysFrameGrabber.h"
#include <EGrabber.h>
#include <common/DetectionConfig.h>


#include "utils/PosixCall.h"
#include "utils/CPUMap.h"
#include <asio.hpp>

#include "ProcessQueueExecuter.h"
#include "StubFrameGrabber.h"

#include <sys/syscall.h>
#include <unistd.h>

struct Options {
	bool PrintHelp;

	DetectionConfig AprilTag2;
	CameraConfiguration       Camera;


	std::string Host;
	uint16_t    Port;
	bool        DrawDetection;
	bool        VideoOutputToStdout;
	size_t      VideoOutputHeight;

	std::string NewAntOuputDir;
	size_t      NewAntROISize;

	std::string frameIDString;
	size_t      FrameStride;
	std::set<uint64> FrameID;


	std::string StubImagePath;
};


void ParseArgs(int & argc, char ** argv,Options & opts ) {
	options::FlagParser parser(options::FlagParser::Default,"low-level vision detection for the FORmicidae Tracker");

	std::string family = "36h11";
	opts.VideoOutputHeight = 1080;
	opts.FrameStride = 1;
	opts.frameIDString = "";
	opts.Port = 3002;
	opts.DrawDetection = false;
	opts.NewAntROISize = 500;
	parser.AddFlag("help",opts.PrintHelp,"Print this help message",'h');

	parser.AddFlag("at-family",family,"The apriltag2 family to use");
	parser.AddFlag("new-ant-roi-size", opts.NewAntROISize, "Size of the image to save when a new ant is found");
	parser.AddFlag("at-quad-decimate",opts.AprilTag2.QuadDecimate,"Decimate original image for faster computation but worse pose estimation. Should be 1.0 (no decimation), 1.5, 2, 3 or 4");
	parser.AddFlag("at-quad-sigma",opts.AprilTag2.QuadSigma,"Apply a gaussian filter for quad detection, noisy image likes a slight filter like 0.8");
	parser.AddFlag("at-refine-edges",opts.AprilTag2.RefineEdges,"Refines the edge of the quad, especially needed if decimation is used, inexpensive");
	parser.AddFlag("at-no-refine-decode",opts.AprilTag2.NoRefineDecode,"Do not refines the tag code detection. Refining is often required for small tags");
	parser.AddFlag("at-refine-pose",opts.AprilTag2.RefinePose,"Refines the pose");
	parser.AddFlag("at-quad-min-cluster",opts.AprilTag2.QuadMinClusterPixel,"Minimum number of pixel to consider it a quad");
	parser.AddFlag("at-quad-max-n-maxima",opts.AprilTag2.QuadMaxNMaxima,"Number of candidate to consider to fit quad corner");
	parser.AddFlag("at-quad-critical-radian",opts.AprilTag2.QuadCriticalRadian,"Rejects quad with angle to close to 0 or 180 degrees");
	parser.AddFlag("at-quad-max-line-mse",opts.AprilTag2.QuadMaxLineMSE,"MSE threshold to reject a fitted quad");
	parser.AddFlag("at-quad-min-bw-diff",opts.AprilTag2.QuadMinBWDiff,"Difference in pixel value to consider a region black or white");
	parser.AddFlag("host", opts.Host, "Host to send tag detection readout");
	parser.AddFlag("port", opts.Port, "Port to send tag detection readout",'p');
	parser.AddFlag("video-to-stdout", opts.VideoOutputToStdout, "Sends video output to stdout");
	parser.AddFlag("video-output-height", opts.VideoOutputHeight, "Video Output height (width computed to maintain aspect ratio");
	parser.AddFlag("new-ant-output-dir",opts.NewAntOuputDir,"Path where to save new detected ant pictures");
	parser.AddFlag("frame-stride",opts.FrameStride,"Frame sequence length");
	parser.AddFlag("frame-ids",opts.frameIDString,"Frame ID to consider in the frame sequence, if empty consider all");
	parser.AddFlag("camera-fps",opts.Camera.FPS,"Camera FPS to use");
	parser.AddFlag("camera-strobe-us",opts.Camera.StrobeDuration,"Camera Strobe Length in us");
	parser.AddFlag("camera-strobe-delay-us",opts.Camera.StrobeDelay,"Camera Strobe Delay in us, negative value allowed");
	parser.AddFlag("camera-slave-mode",opts.Camera.Slave,"Use the camera in slave mode (CoaXPress Data Forwarding)");
	parser.AddFlag("draw-detection",opts.DrawDetection,"Draw detection on the output if activated");
	parser.AddFlag("stub-image-path", opts.StubImagePath, "Use a stub image instead of an actual framegrabber");


	parser.Parse(argc,argv);

	opts.AprilTag2.Family = DetectionConfig::FamilyTypeFromName(family);

	if (opts.PrintHelp == true) {
		parser.PrintUsage(std::cerr);
		exit(0);
	}
	if (opts.FrameStride == 0 ) {
		opts.FrameStride = 1;
	}
	if (opts.FrameStride > 100 ) {
		throw std::invalid_argument("Frame stride to big, max is 100");
	}

	if ( opts.frameIDString.empty() ) {
		for(size_t i = 0; i < opts.FrameStride; ++i ) {
			opts.FrameID.insert(i);
		}
	} else {
		std::vector<std::string> IDs;
		base::SplitString(opts.frameIDString.cbegin(),
		                  opts.frameIDString.cend(),
		                  ",",
		                  std::back_inserter<std::vector<std::string>>(IDs));
		for (auto IDstr : IDs) {
			std::istringstream is(base::TrimSpaces(IDstr));
			uint64_t ID;
			is >> ID;
			if ( !is.good() && is.eof() == false ) {
				std::ostringstream os;
				os << "Cannot parse '" << IDstr << "'  in  '" << opts.frameIDString << "'";
				throw std::runtime_error(os.str());
			}
			if ( ID >= opts.FrameStride ) {
				std::ostringstream os;
				os << "Frame ID (" << ID << ") cannot be superior to frame stride (" << opts.FrameStride << ")";
				throw std::runtime_error(os.str());
			}
			opts.FrameID.insert(ID);
		}
	}
}


void Execute(int argc, char ** argv) {
	Options opts;
	ParseArgs(argc, argv,opts);

	FLAGS_stderrthreshold = 0;
	if ( opts.VideoOutputToStdout ) {
		FLAGS_stderrthreshold = 4;
	}
	::google::InitGoogleLogging(argv[0]);
	::google::InstallFailureSignalHandler();



	std::vector<CPUID> ioCPUs;
	std::vector<CPUID> workCPUs;
	auto coremap = GetCoreMap();
	if ( coremap.size() == 1 ) {
		ioCPUs.push_back(coremap[0].CPUs[0]);
		if (coremap[0].CPUs.size() == 0 ) {
			workCPUs.push_back(coremap[0].CPUs[0]);
		} else {
			for ( size_t i = 1; i < coremap[0].CPUs.size(); ++i ) {
				workCPUs.push_back(coremap[0].CPUs[i]);
			}
		}
	} else {
		for( size_t i = 0; i < std::min(coremap[0].CPUs.size(),(size_t)2); ++i ) {
			DLOG(INFO) << "IO Core: " << coremap[0].ID << " CPU: " << coremap[0].CPUs[i];
			ioCPUs.push_back(coremap[0].CPUs[i]);
		}
		for (size_t i = 2; i < coremap[0].CPUs.size(); ++i ) {
			DLOG(INFO) << "WORK Core: " << coremap[0].ID << " CPU: " << coremap[0].CPUs[i];
			workCPUs.push_back(coremap[0].CPUs[i]);
		}
		for (size_t i = 1; i < coremap.size(); ++i ) {
			for ( size_t j = 0 ; j < coremap[i].CPUs.size(); ++j ) {
				DLOG(INFO) << "WORK Core: " << coremap[i].ID << " CPU: " << coremap[i].CPUs[j];
				workCPUs.push_back(coremap[i].CPUs[j]);
			}
		}
	}


	asio::io_service io;

	Connection::Ptr connection;
	if (!opts.Host.empty()) {
		connection = Connection::Create(io,opts.Host,opts.Port);
	}

	//creates queues
	PreTagQueue  preTag;
	PostTagQueue postTag;


	if ( !opts.NewAntOuputDir.empty() ) {
		auto acp = std::make_shared<AntCataloguerProcess>(io,opts.NewAntOuputDir,opts.NewAntROISize);
		postTag.push_back([acp](const Frame::Ptr & frame,const cv::Mat & upstream,const fort::FrameReadout & readout,cv::Mat & result){ (*acp)(frame,upstream,readout,result); });
	}
	//queues when outputting data
	if (opts.VideoOutputToStdout) {
		auto rp = std::make_shared<ResizeProcess>(opts.VideoOutputHeight);
		preTag.push_back([rp](const Frame::Ptr & frame,const cv::Mat & upstream,cv::Mat & result){ (*rp)(frame,upstream,result);});
		if (opts.DrawDetection ) {
			auto ddp = std::make_shared<DrawDetectionProcess>();
			postTag.push_back([ddp](const Frame::Ptr & frame,const cv::Mat & upstream,const fort::FrameReadout & readout,cv::Mat & result){ (*ddp)(frame,upstream,readout,result); });
		}
		auto op = std::make_shared<OutputProcess>(io);
		postTag.push_back([op](const Frame::Ptr & frame,const cv::Mat & upstream,const fort::FrameReadout & readout,cv::Mat & result){ (*op)(frame,upstream,readout,result); });
	}

	std::shared_ptr<FrameGrabber> fg;
	std::shared_ptr<Euresys::EGenTL> gentl;
	if (opts.StubImagePath.empty() ) {
		gentl = std::make_shared<Euresys::EGenTL>();
		fg = std::make_shared<EuresysFrameGrabber>(*gentl,opts.Camera);
	} else {
		fg = std::make_shared<StubFrameGrabber>(opts.StubImagePath);
	}

	ProcessQueueExecuter executer(fg->FrameSize(),opts.AprilTag2,workCPUs,preTag,postTag,connection);



	//Stops on SIGINT
	asio::signal_set signals(io,SIGINT);
	signals.async_wait([&io,&executer](const asio::error_code &,
	                         int ) {
		                   LOG(INFO) << "Terminating (SIGINT)";
		                   io.stop();
		                   executer.Stop();
	                   });


	fort::FrameReadout error;

	std::function<void()> WaitForFrame = [&WaitForFrame,&io,&executer,&fg,&opts,&error,connection](){
		Frame::Ptr f = fg->NextFrame();
		if ( opts.FrameStride > 1 ) {
			uint64_t IDInStride = f->ID() % opts.FrameStride;
			if (opts.FrameID.count(IDInStride) == 0 ) {
				io.post(WaitForFrame);
				return;
			}
		}

		try {
			executer.Push(f);
			DLOG(INFO) << "Processing frame " << f->ID();
		} catch ( const ProcessQueueExecuter::Overflow & e ) {
			LOG(WARNING) << "Process overflow : skipping frame " << f->ID() <<  " state: " << executer.State();
			executer.RestartBrokenChilds();
			if (connection) {
				error.Clear();
				error.set_timestamp(f->Timestamp());
				error.set_frameid(f->ID());
				auto time = error.mutable_time();
				time->set_seconds(f->Time().tv_sec);
				time->set_nanos(f->Time().tv_usec*1000);
				error.set_error(fort::FrameReadout::PROCESS_OVERFLOW);

				Connection::PostMessage(connection,error);
			}
		}

		io.post(WaitForFrame);
	};

	fg->Start();
	io.post(WaitForFrame);

	std::vector<std::thread> ioThreads;

	for ( size_t i = 0; i < ioCPUs.size(); ++i) {
		cpu_set_t cpuset;
		CPU_ZERO(&cpuset);
		CPU_SET(ioCPUs[i],&cpuset);
		DLOG(INFO) << "Spawning io on CPU " << ioCPUs[i];
		ioThreads.push_back(std::thread([&io]() {
					io.run();
				}));
		p_call(pthread_setaffinity_np,ioThreads.back().native_handle(),sizeof(cpu_set_t),&cpuset);
	}

	executer.Loop();

	for (auto & t : ioThreads ) {
		t.join();
	}


	fg->Stop();
}

int main(int argc, char ** argv) {
	try {
		Execute(argc,argv);
	} catch (const std::exception & e) {
		LOG(ERROR) << "Got uncaught exception: " << e.what();
		return 1;
	}
}
