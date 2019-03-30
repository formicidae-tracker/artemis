#include <glog/logging.h>

#include "Apriltag2Process.h"
#include "ResizeProcess.h"
#include "utils/FlagParser.h"
#include "utils/StringManipulation.h"
#include "EuresysFrameGrabber.h"
#include <EGrabber.h>

#include "utils/PosixCall.h"

#include <asio.hpp>
#include "EventManager.h"


struct Options {
	bool PrintHelp;

	AprilTag2Detector::Config AprilTag2;
	CameraConfiguration       Camera;


	std::string HermesAddress;
	bool        VideoOutputToStdout;
	size_t      VideoOutputHeight;
	std::string NewAntOuputDir;

	std::string frameIDString;
	uint64      FrameStride;
	std::set<uint64> FrameID;
};


void ParseArgs(int & argc, char ** argv,Options & opts ) {
	options::FlagParser parser(options::FlagParser::Default,"low-level vision detection for the FORmicidae Tracker");

	AprilTag2Detector::Config c;
	opts.VideoOutputHeight = 1080;
	opts.FrameStride = 1;
	opts.frameIDString = "";

	parser.AddFlag("help",opts.PrintHelp,"Print this help message",'h');

	parser.AddFlag("at-family",opts.AprilTag2.Family,"The apriltag2 family to use");
	parser.AddFlag("new-ant-roi-size", opts.AprilTag2.NewAntROISize, "Size of the image to save when a new ant is found");
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
	parser.AddFlag("at-quad-deglitch",opts.AprilTag2.QuadDeglitch,"Deglitch only for noisy images");
	parser.AddFlag("address", opts.HermesAddress, "Address to send tag detection readout",'a');
	parser.AddFlag("video-to-stdout", opts.VideoOutputToStdout, "Sends video output to stdout");
	parser.AddFlag("video-output-height", opts.VideoOutputHeight, "Video Output height (width computed to maintain aspect ratio");
	parser.AddFlag("new-ant-output-dir",opts.NewAntOuputDir,"Path where to save new detected ant pictures");
	parser.AddFlag("frame-stride",opts.FrameStride,"Frame sequence length");
	parser.AddFlag("frame-ids",opts.frameIDString,"Frame ID to consider in the frame sequence, if empty consider all");
	parser.AddFlag("camera-fps",opts.Camera.FPS,"Camera FPS to use");
	parser.AddFlag("camera-exposure-us",opts.Camera.ExposureTime,"Camera Exposure time in us");
	parser.AddFlag("camera-strobe-us",opts.Camera.ExposureTime,"Camera Strobe Length in us");
	parser.AddFlag("camera-strobe-offset-us",opts.Camera.ExposureTime,"Camera Strobe Offset in us, negative value allowed");
	parser.AddFlag("camera-slave-mode",opts.Camera.Slave,"Use the camera in slave mode (CoaXPress Data Forwarding)");


	parser.Parse(argc,argv);
	if (opts.PrintHelp == true) {
		parser.PrintUsage(std::cerr);
		exit(0);
	}
	if (opts.FrameStride == 0 ) {
		opts.FrameStride = 1;
	}
	if (opts.FrameStride > 100 ) {
		throw std::invalid_argument("Frame stride to big, mas is 100");
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

std::shared_ptr<asio::ip::tcp::socket> Connect(asio::io_service & io, const std::string & address) {
		asio::ip::tcp::resolver resolver(io);
		asio::ip::tcp::resolver::query q(address.c_str(),"artemis");
		asio::ip::tcp::resolver::iterator endpoint = resolver.resolve(q);
		auto res = std::make_shared<asio::ip::tcp::socket>(io);
		asio::connect(*res,endpoint);
}


void Execute(int argc, char ** argv) {
	::google::InitGoogleLogging(argv[0]);
	Options opts;
	ParseArgs(argc, argv,opts);

	//creates queues
	auto messages = SerializedMessageBuffer::Create();
	auto messagesProducer = std::move(messages.first);
	auto messagesConsumer = std::move(messages.second);

	auto newAnts = UnknownAntsBuffer::Create();
	auto newAntsProducer = std::move(newAnts.first);
	auto newAntsConsumer = std::move(newAnts.second);

	ProcessQueue pq = AprilTag2Detector::Create(opts.AprilTag2,
	                                            messagesProducer,
	                                            newAntsProducer);

	if (opts.VideoOutputToStdout) {
		pq.push_back(std::make_shared<ResizeProcess>(opts.VideoOutputHeight));
	}

	asio::io_service io;
	auto eventManager = EventManager::Create(io);

	std::shared_ptr<asio::ip::tcp::socket> upstream;
	if ( !opts.HermesAddress.empty() ) {
		upstream = Connect(io,opts.HermesAddress);
	}

	Euresys::EGenTL gentl;

	EuresysFrameGrabber fg(gentl,opts.Camera,eventManager);

	//install event handler
	EventManager::Handler eHandler = [upstream,&messagesConsumer,&newAntsConsumer,&opts,&io](Event e) {
		switch(e) {
		case Event::FRAME_READY: {

			break;
		}
		case Event::NEW_READOUT: {
			try {
				if (!upstream) {
					messagesConsumer->Pop();
				}

				asio::async_write<asio::ip::tcp::socket,

				(*upstream,messagesConsumer->Head(),[&messagesConsumer](const asio::error_code & ec,
					   std::size_t ) {
					                  if(ec) {
						                  LOG(ERROR) << "Could not send message upstream";
					                  }
					                  try {
						                  messagesConsumer->Pop();
					                  } catch ( const SerializedMessageBuffer::EmptyException & ) {
						                  LOG(ERROR) << "internal error: MessageBuffer should not be empty";
					                  }
				                  });


			} catch ( const SerializedMessageBuffer::EmptyException &) {
				LOG(ERROR) << "Internal Error: MessageBuffer should not be empty";
			}
			break;
		}
		case Event::NEW_ANT_DISCOVERED: {
			try{
				if ( opts.NewAntOuputDir.empty() ) {
					newAntsConsumer->Pop();
				}
				std::ostringstream path(opts.NewAntOuputDir);
				auto a = newAntsConsumer->Head();

				path << "/" << "ant_" << a.ID << ".png";
				int fd = open(path.str().c_str(),O_CREAT|O_WRONLY|O_NONBLOCK);
				if ( fd != -1) {
					auto file = std::shared_ptr<asio::posix::stream_descriptor>(new asio::posix::stream_descriptor(io,fd),[](asio::posix::stream_descriptor * sd) {
							close(sd->native_handle());
							delete sd;
						});

					asio::async_write(*file,asio::buffer(&((*(a.PNGData))[0]),a.PNGData->size()),[file,&newAntsConsumer](const asio::error_code & ec,
						                               std::size_t ){
						                  if(ec) {
							                  LOG(ERROR) << "Could not save png data";
						                  }
						                  try {
							                  newAntsConsumer->Pop();
						                  } catch ( const UnknownAntsBuffer::EmptyException & ) {
							                  LOG(ERROR) << "Internal Error: NewAntBuffer should not be empty";
						                  }
					                  });
				} else {
					LOG(ERROR) << "Could not create '" << path.str() << "': open: " << std::error_code(errno,ARTEMIS_SYSTEM_CATEGORY());
				}

				} catch ( const UnknownAntsBuffer::EmptyException &) {
					LOG(ERROR) << "Internal Error: NewAntBuffer should not be empty";
				}
			}
		break;
		}

	};




}

int main(int argc, char ** argv) {
	try {
		Execute(argc,argv);
	} catch (const std::exception & e) {
		LOG(ERROR) << "Got uncaught exception: " << e.what();
		return 1;
	}
}
