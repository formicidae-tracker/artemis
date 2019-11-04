#include <glog/logging.h>

#include <thread>

#include <artemis-config.h>

#include "Apriltag2Process.h"
#include "ResizeProcess.h"
#include "OutputProcess.h"
#include "AntCataloguerProcess.h"
#include "LegacyAntCataloguerProcess.h"
#include "DrawDetectionProcess.h"
#include "WatermarkingProcess.h"
#include "FrameDisplayer.h"
#include "OverlayWriter.h"
#include "utils/FlagParser.h"
#include "utils/StringManipulation.h"
#include "CameraConfiguration.h"

#ifndef FORCE_STUB_FRAMEGRABBER_ONLY
#include "EuresysFrameGrabber.h"
#include <EGrabber.h>
#endif // FORCE_STUB_FRAMEGRABBER_ONLY

#include <Eigen/Core>

#include "utils/PosixCall.h"
#include "utils/CPUMap.h"
#include <asio.hpp>

#include "ProcessQueueExecuter.h"
#include "StubFrameGrabber.h"

#include <sys/syscall.h>
#include <unistd.h>

#include "artemis-config.h"

struct Options {
	bool PrintHelp;
	bool PrintVersion;

	AprilTag2Detector::Config AprilTag2;
	CameraConfiguration       Camera;


	std::string Host;
	std::string UUID;
	uint16_t    Port;
	bool        DrawDetection;
	bool        DrawStatistics;
	bool        DisplayOutput;
	bool        VideoOutputToStdout;
	bool        LegacyMode;

	size_t      VideoOutputHeight;
	bool        VideoOutputAddHeader;

	std::string NewAntOuputDir;
	size_t      NewAntROISize;
	double      AntRenewPeriodHours;

	std::string frameIDString;
	size_t      FrameStride;
	std::set<uint64> FrameID;


	std::string StubImagePath;
	std::string LogDir;

	bool        TestMode;
};


void ParseArgs(int & argc, char ** argv,Options & opts ) {
	options::FlagParser parser(options::FlagParser::Default,"low-level vision detection for the FORmicidae Tracker");

	AprilTag2Detector::Config c;
	opts.VideoOutputHeight = 1080;
	opts.FrameStride = 1;
	opts.frameIDString = "";
	opts.Port = 3002;
	opts.DrawDetection = false;
	opts.DisplayOutput = false;
	opts.DrawStatistics = false;
	opts.VideoOutputAddHeader = false;
	opts.NewAntROISize = 500;
	opts.AntRenewPeriodHours = 2;
	opts.PrintVersion = false;
	opts.LegacyMode = false;
	opts.TestMode = false;
	parser.AddFlag("help",opts.PrintHelp,"Print this help message",'h');
	parser.AddFlag("version",opts.PrintVersion,"Print version");

	parser.AddFlag("at-family",opts.AprilTag2.Family,"The apriltag2 family to use");
	parser.AddFlag("at-quad-decimate",opts.AprilTag2.QuadDecimate,"Decimate original image for faster computation but worse pose estimation. Should be 1.0 (no decimation), 1.5, 2, 3 or 4");
	parser.AddFlag("at-quad-sigma",opts.AprilTag2.QuadSigma,"Apply a gaussian filter for quad detection, noisy image likes a slight filter like 0.8");
	parser.AddFlag("at-refine-edges",opts.AprilTag2.RefineEdges,"Refines the edge of the quad, especially needed if decimation is used, inexpensive");
	parser.AddFlag("at-quad-min-cluster",opts.AprilTag2.QuadMinClusterPixel,"Minimum number of pixel to consider it a quad");
	parser.AddFlag("at-quad-max-n-maxima",opts.AprilTag2.QuadMaxNMaxima,"Number of candidate to consider to fit quad corner");
	parser.AddFlag("at-quad-critical-radian",opts.AprilTag2.QuadCriticalRadian,"Rejects quad with angle to close to 0 or 180 degrees");
	parser.AddFlag("at-quad-max-line-mse",opts.AprilTag2.QuadMaxLineMSE,"MSE threshold to reject a fitted quad");
	parser.AddFlag("at-quad-min-bw-diff",opts.AprilTag2.QuadMinBWDiff,"Difference in pixel value to consider a region black or white");
	parser.AddFlag("at-quad-deglitch",opts.AprilTag2.QuadDeglitch,"Deglitch only for noisy images");
	parser.AddFlag("host", opts.Host, "Host to send tag detection readout");
	parser.AddFlag("port", opts.Port, "Port to send tag detection readout",'p');
	parser.AddFlag("uuid",opts.UUID,"The UUID to mark data sent over network");
	parser.AddFlag("video-to-stdout", opts.VideoOutputToStdout, "Sends video output to stdout");
	parser.AddFlag("video-output-height", opts.VideoOutputHeight, "Video Output height (width computed to maintain aspect ratio");
	parser.AddFlag("video-output-add-header", opts.VideoOutputAddHeader, "Adds binary header to stdout output");

	parser.AddFlag("new-ant-output-dir",opts.NewAntOuputDir,"Path where to save new detected ant pictures");
	parser.AddFlag("new-ant-roi-size", opts.NewAntROISize, "Size of the image to save when a new ant is found");
	parser.AddFlag("ant-renew-period-hour", opts.AntRenewPeriodHours, "Renew ant cataloguing every X hours");


	parser.AddFlag("frame-stride",opts.FrameStride,"Frame sequence length");
	parser.AddFlag("frame-ids",opts.frameIDString,"Frame ID to consider in the frame sequence, if empty consider all");
	parser.AddFlag("camera-fps",opts.Camera.FPS,"Camera FPS to use");
	parser.AddFlag("camera-strobe-us",opts.Camera.StrobeDuration,"Camera Strobe Length in us");
	parser.AddFlag("camera-strobe-delay-us",opts.Camera.StrobeDelay,"Camera Strobe Delay in us, negative value allowed");
	parser.AddFlag("camera-slave-mode",opts.Camera.Slave,"Use the camera in slave mode (CoaXPress Data Forwarding)");
	parser.AddFlag("draw-detection",opts.DrawDetection,"Draw detection on the output if activated");
	parser.AddFlag("display-output", opts.DisplayOutput, "Display locally the detection. Implies --draw-detection and --draw-statistic",'d');
	parser.AddFlag("draw-statistics", opts.DrawStatistics, "Draw statistics on the output frames");

#ifndef FORCE_STUB_FRAMEGRABBER_ONLY
	parser.AddFlag("stub-image-path", opts.StubImagePath, "Use a stub image instead of an actual framegrabber");
#else
	parser.AddFlag("stub-image-path", opts.StubImagePath, "Use a stub image instead of an actual framegrabber",
	               options::FlagParser::NO_SHORT,true);
#endif // FORCE_STUB_FRAMEGRABBER_ONLY

	parser.AddFlag("legacy-mode",opts.LegacyMode,"Uses a legacy mode data output for ants cataloguing");
	parser.AddFlag("test-mode",opts.TestMode,"Test mode, adds an overlay detection drawing and statistics");

	parser.AddFlag("log-output-dir",opts.LogDir,"Directory to put current logs in");

	parser.Parse(argc,argv);

	if (opts.PrintHelp == true) {
		parser.PrintUsage(std::cerr);
		exit(0);
	}
	if (opts.PrintVersion == true ) {
		std::cout << "artemis v" << ARTEMIS_VERSION << std::endl;
		exit(0);
	}

	if (opts.FrameStride == 0 ) {
		opts.FrameStride = 1;
	}
	if (opts.FrameStride > 100 ) {
		throw std::invalid_argument("Frame stride to big, max is 100");
	}

	if (opts.TestMode == true ) {
		opts.DrawDetection = true;
		opts.DrawStatistics = true;
		opts.DisplayOutput = true;
	}

	if (opts.AntRenewPeriodHours < 0.25 ) {
		throw std::invalid_argument("Ant renew period is too small, min 0.25 hour");
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
	Eigen::initParallel();
	cv::setNumThreads(1);
	Options opts;
	ParseArgs(argc, argv,opts);

	if (opts.LogDir.empty() == false ) {
		FLAGS_log_dir = opts.LogDir.c_str();
	}

	FLAGS_stderrthreshold = 0;
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
	asio::io_service workload;
	asio::io_service::work work(workload);

	//Stops on SIGINT
	asio::signal_set signals(io,SIGINT);
	signals.async_wait([&io,&workload](const asio::error_code &,
	                         int ) {
		                   LOG(INFO) << "Terminating (SIGINT)";
		                   io.stop();
		                   workload.stop();
	                   });

	Connection::Ptr connection;
	if (!opts.Host.empty()) {
		connection = Connection::Create(io,opts.Host,opts.Port);
	}



	//creates queues
	ProcessQueue pq = AprilTag2Detector::Create(workCPUs.size(),
	                                            opts.AprilTag2,
	                                            connection,
	                                            opts.UUID);

	if ( !opts.NewAntOuputDir.empty() ) {
		if ( opts.LegacyMode == false ) {
			pq.push_back(std::make_shared<AntCataloguerProcess>(opts.NewAntOuputDir,opts.NewAntROISize,std::chrono::seconds((long)(opts.AntRenewPeriodHours*3600))));
		} else {
			pq.push_back(std::make_shared<LegacyAntCataloguerProcess>(opts.NewAntOuputDir,std::chrono::seconds((long)(opts.AntRenewPeriodHours*3600))));
		}
	}

	bool desactivateQuitFromWindow = false;
	std::string watermark = "";
	if ( opts.TestMode == false ) {
		//we are in production mode we simply desactivate quitting from UI.
		LOG(INFO) << "We are producing data, the UI are not authorized to quit application";
		desactivateQuitFromWindow = true;
	} else {
		LOG(WARNING) << "Test mode: no data will be saved";
		watermark = "TEST MODE";
		desactivateQuitFromWindow = true;
	}

	//queues when outputting data

	std::shared_ptr<OverlayWriter> oWriter;
	if (opts.VideoOutputToStdout || opts.DisplayOutput) {
		bool forceIntergerSclaing = opts.LegacyMode;

		pq.push_back(std::make_shared<ResizeProcess>(opts.VideoOutputHeight,forceIntergerSclaing));
		oWriter = std::make_shared<OverlayWriter>(opts.DrawStatistics);
		pq.push_back(oWriter);
		if  (! watermark.empty() ) {
			pq.push_back(std::make_shared<WatermarkingProcess>(watermark));
		}
	}

	if ( opts.VideoOutputToStdout ) {
		pq.push_back(std::make_shared<OutputProcess>(io,opts.VideoOutputAddHeader));
	}

	std::shared_ptr<DrawDetectionProcess> drawDetections;

	if ( opts.DrawDetection == true) {
		drawDetections = std::make_shared<DrawDetectionProcess>();
		pq.push_back(drawDetections);
	}

	if ( opts.DisplayOutput ) {
		pq.push_back(std::make_shared<FrameDisplayer>(desactivateQuitFromWindow,drawDetections,oWriter));
	}


	std::shared_ptr<FrameGrabber> fg;
#ifndef FORCE_STUB_FRAMEGRABBER_ONLY
	std::shared_ptr<Euresys::EGenTL> gentl;
	if (opts.StubImagePath.empty() ) {
		gentl = std::make_shared<Euresys::EGenTL>();
		fg = std::make_shared<EuresysFrameGrabber>(*gentl,opts.Camera);
	} else {
		fg = std::make_shared<StubFrameGrabber>(opts.StubImagePath);
	}
#else
	fg = std::make_shared<StubFrameGrabber>(opts.StubImagePath);
#endif

	ProcessQueueExecuter executer(workload,workCPUs.size());


	fort::hermes::FrameReadout error;

	std::function<void()> WaitForFrame = [&WaitForFrame,&io,&executer,&pq,&fg,&opts,&error,connection](){
		Frame::Ptr f = fg->NextFrame();
		if ( opts.FrameStride > 1 ) {
			uint64_t IDInStride = f->ID() % opts.FrameStride;
			if (opts.FrameID.count(IDInStride) == 0 ) {
				io.post(WaitForFrame);
				return;
			}
		}

		try {
			executer.Start(pq,f);
			DLOG(INFO) << "Processing frame " << f->ID();
		} catch ( const ProcessQueueExecuter::Overflow & e ) {
			LOG(WARNING) << "Process overflow : skipping frame " << f->ID() <<  " state: " << executer.State();

			if (connection) {
				error.Clear();
				error.set_timestamp(f->Timestamp());
				error.set_frameid(f->ID());
				auto time = error.mutable_time();
				time->set_seconds(f->Time().tv_sec);
				time->set_nanos(f->Time().tv_usec*1000);
				error.set_error(fort::hermes::FrameReadout::PROCESS_OVERFLOW);
				error.set_producer_uuid(opts.UUID);
				Connection::PostMessage(connection,error);
			}
		}

		io.post(WaitForFrame);
	};

	fg->Start();
	io.post(WaitForFrame);

	std::vector<std::thread> workThreads,ioThreads;

	for(size_t i = 0; i < workCPUs.size(); ++i) {
		cpu_set_t cpuset;
		CPU_ZERO(&cpuset);
		CPU_SET(workCPUs[i],&cpuset);
		DLOG(INFO) << "Spawning worker on CPU " << workCPUs[i];
		workThreads.push_back(std::thread([&workload](){
					workload.run();
				}));
		//p_call(pthread_setaffinity_np,workThreads.back().native_handle(),sizeof(cpu_set_t),&cpuset);
	}

	for ( size_t i = 0; i < ioCPUs.size(); ++i) {
		cpu_set_t cpuset;
		CPU_ZERO(&cpuset);
		CPU_SET(ioCPUs[i],&cpuset);
		DLOG(INFO) << "Spawning io on CPU " << ioCPUs[i];
		ioThreads.push_back(std::thread([&io]() {
					io.run();
				}));
		//p_call(pthread_setaffinity_np,ioThreads.back().native_handle(),sizeof(cpu_set_t),&cpuset);
	}

	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(ioCPUs[0],&cpuset);

	long int tid = syscall(SYS_gettid);


	for (auto & t : ioThreads ) {
		t.join();
	}
	fg->Stop();

	for( auto & t : workThreads) {
		t.join();
	}

}

int main(int argc, char ** argv) {
	try {
		Execute(argc,argv);
	} catch (const std::exception & e) {
		LOG(ERROR) << "Got uncaught exception: " << e.what();
		return 1;
	}
}
