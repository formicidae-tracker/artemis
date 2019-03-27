#include "FileWriter.h"

#include <fstream>

#include <glog/logging.h>


FileWriter::FileWriter()
	: d_quit(false)
	, d_thread(&FileWriter::Loop,this) {
}

FileWriter::~FileWriter() {
	d_quit.store(true);
	d_thread.join();
}

void FileWriter::Write(const std::string & path,
                       const std::shared_ptr<std::vector<uint8_t > > & data) {

	std::lock_guard<std::mutex> lock(d_mutex);
	if (d_data || path.size() != 0) {
		throw std::runtime_error("busy");
	}

	d_data = data;
	d_path = path;
	d_signal.notify_all();

}

void FileWriter::Loop() {
	while(d_quit.load() == false ) {
		std::shared_ptr< std::vector<uint8_t> > data;
		std::string path;
		{
			std::unique_lock<std::mutex> lock;
			d_signal.wait(lock,[this,&data,&path]() -> bool {
					if (d_quit.load() == true ) {
						return true;
					}
					if (!d_data || d_path.size() == 0 ) {
						return false;
					}
					data = d_data;
					path = d_path;
					d_data.reset();
					d_path.erase();
					return true;
				});

			if (d_quit.load() == true ) {
				return;
			}
		}

		std::ofstream file(path.c_str());
		file.write(reinterpret_cast<char*>(&((*data)[0])),data->size());
		if (!file.good()) {
			LOG(ERROR) << "Could not write '" << path << "'";
		}
	}
}
