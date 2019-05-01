#include "ProcessQueueExecuter.h"

#include <glog/logging.h>

#include <unistd.h>

#include "utils/StringManipulation.h"
#include <sys/types.h>
#include <sys/stat.h>
#include "utils/PosixCall.h"

#define MAX_RESPAWN 100000

std::string ProcessQueueExecuter::FindOrionPath() {
	char buffer[1024];
	int size = readlink("/proc/self/exe",buffer,1024);
	if ( size == -1 ) {
		throw ARTEMIS_SYSTEM_ERROR(readlink, errno);
	}

	if (size < 1024 ) {
		buffer[size] = 0;
	} else {
		throw std::runtime_error("Path too long");
	}

	std::string artemisPath(buffer,size);
	std::string basepath = base::TrimSuffix(artemisPath,"/artemis");

	//first test if in same directory
	std::vector<std::string> guesses = { "/orion","/../orion/orion" } ;

	for(auto const & g : guesses ) {
		std::string path = basepath + g;
		try {
			struct stat s;
			p_call(stat,path.c_str(),&s);
			if (s.st_mode & S_IEXEC ) {
				LOG(INFO) << "Found orion: " << path;
				return path;
			}
		} catch (const std::system_error & e) {
			DLOG(INFO) << "stat(" << path << "): " << e.what();
			continue;
		}
	}

	throw std::runtime_error("Could not find path to orion executable");
}


ProcessQueueExecuter::ProcessQueueExecuter(const cv::Size & size,
                                           const DetectionConfig & config,
                                           const std::vector<CPUID> cpus,
	                                       const PreTagQueue & preTagQueue,
                                           const PostTagQueue & postTagQueue,
                                           const Connection::Ptr & connection)
	: d_orion(FindOrionPath())
	, d_preTagQueue(preTagQueue)
	, d_postTagQueue(postTagQueue)
	, d_quit(false)
	, d_connection(connection)
	, d_respawn(0) {

	PartitionRectangle(cv::Rect(cv::Point(0,0),size),cpus.size(),d_partition);
	AddMargin(size,75,d_partition);
	d_manager = std::make_shared<InterprocessManager>(true);

	for( auto const & cpu : cpus) {
		auto const & p = d_partition[d_buffers.size()];
		d_buffers.push_back(std::unique_ptr<InterprocessBuffer>( new InterprocessBuffer(d_manager,d_buffers.size(),p,config)));
		std::ostringstream os;
		os << d_children.size();
		d_children.push_back(std::unique_ptr<ChildProcess>( new ChildProcess(cpu,d_orion,os.str())));
	}

}

ProcessQueueExecuter::~ProcessQueueExecuter() {
	Stop();
}

void ProcessQueueExecuter::Push(const Frame::Ptr & current) {
	std::lock_guard<std::mutex> lock(d_mutex);
	if ( d_children.empty() ) {
		throw std::runtime_error("stopped");
	}
	if ( d_next ) {
		throw Overflow();
	}
	if (d_frame) {
		d_next = current;
		return;
	}
	d_frame = current;
	d_start.notify_all();
}


void ProcessQueueExecuter::Loop() {
	fort::FrameReadout m;
	for (;;) {
		{
			std::unique_lock<std::mutex> lock(d_mutex);
			d_start.wait(lock,[this]()->bool {
					if (d_quit == true){
						return true;
					}

					if ( d_frame ) {
						return true;
					}
					return false;
				});

			if (d_quit == true ) {
				return;
			}
		}

		//partition, start workers,
		size_t i = 0;
		for(auto const & roi : d_partition ) {
			cv::Mat(d_frame->ToCV(),roi).copyTo(d_buffers[i]->Image());
			d_buffers[i]->TimestampIn() = d_frame->Timestamp();
			++i;
		}

		d_manager->PostNewJob(d_frame->Timestamp());

		//starts to
		cv::Mat current = d_frame->ToCV();
		cv::Mat result;
		for( const auto & j : d_preTagQueue ) {
			result = current;
			j(d_frame,current,result);
			current = result;
		}

		d_manager->WaitAllFinished();

		std::set<uint16_t> IDs;
		m.Clear();
		m.set_timestamp(d_frame->Timestamp());
		m.set_frameid(d_frame->ID());
		auto time = m.mutable_time();
		time->set_seconds(d_frame->Time().tv_sec);
		time->set_nanos(d_frame->Time().tv_usec*1000);
		int idx = -1;
		for(auto & buffer : d_buffers ) {
			++idx;
			size_t xOffset = d_partition[idx].x;
			size_t yOffset = d_partition[idx].y;
			if ( buffer->TimestampOut() != d_frame->Timestamp() ) {
				LOG(ERROR) << "Skipping worker result: timestamp mismatch, got:" << buffer->TimestampOut() << " expected: " << d_frame->Timestamp();
				continue;
			}

			for ( size_t i = 0; i < buffer->DetectionsSize(); ++i ) {
				auto const & d = buffer->Detections()[i];
				if ( IDs.count(d.ID) != 0 ) {
					continue;
				}
				auto a = m.add_ants();
				a->set_id(d.ID);
				a->set_x(d.X + xOffset);
				a->set_y(d.Y + yOffset);
				a->set_theta(d.Theta);
			}
		}

		if (d_connection) {
			Connection::PostMessage(d_connection,m);
		}

		for(auto const & j : d_postTagQueue ) {
			result = current;
			j(d_frame,current,m,result);
			current = result;
		}

		{
			std::lock_guard<std::mutex> lock(d_mutex);
			if ( d_next ) {
				d_frame = d_next;
				d_next.reset();
				continue;
			}
			d_frame.reset();
		}
	}

}


void ProcessQueueExecuter::Stop() {
	std::lock_guard<std::mutex> lock(d_mutex);
	d_children.clear();
	d_quit = true;
	d_start.notify_all();
}

std::string ProcessQueueExecuter::State() {
	return "NOT YET IMPLEMENTED";
}

void ProcessQueueExecuter::RestartBrokenChilds() {
	std::lock_guard<std::mutex> lock(d_mutex);
	bool respawned = false;
	for ( std::size_t i = 0; i < d_children.size(); ++i) {
		if ( d_children[i]->Ended() == false ) {
			continue;
		}
		LOG(ERROR) << "Child process " << i << " exited with code " << d_children[i]->Status() << " respawning";
		if ( ++d_respawn > MAX_RESPAWN ) {
			throw std::runtime_error("Too many children respawn");
		}
		auto cpu = d_children[i]->CPU();
		std::ostringstream os;
		os << i ;
		d_children[i] = std::unique_ptr<ChildProcess>(new ChildProcess(cpu,d_orion,os.str()));
		respawned = true;
	}

	if ( respawned == false ) {
		return;
	}
	LOG(ERROR) << "Skipping frame " << d_frame->ID() << " because of failed child";
	if ( d_next ) {
		d_frame =  d_next;
		d_next.reset();
		return;
	}
	d_frame.reset();
}
