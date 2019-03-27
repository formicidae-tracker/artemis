#pragma once


#include <string>
#include <vector>
#include <memory>
#include <condition_variable>
#include <atomic>
#include <thread>



class FileWriter {
public:
	FileWriter();
	~FileWriter();

	void Write(const std::string & path, const std::shared_ptr<std::vector<uint8_t> > & data);
	bool IsDone();

private:
	void Loop();

	std::atomic<bool> d_quit;
	std::shared_ptr<std::vector<uint8_t> > d_data;
	std::string d_path;

	std::mutex d_mutex;
	std::condition_variable d_signal;

	std::thread d_thread;
};
