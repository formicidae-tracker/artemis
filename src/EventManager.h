#pragma once

#include <signal.h>

#include <memory>
#include <atomic>
#include <iostream>


#include <asio/posix/stream_descriptor.hpp>
#include <asio/io_service.hpp>

enum class Event {
	QUIT = 0,
	FRAME_READY,
	PROCESS_NEED_REFRESH,
	NB_EVENTS
};


class EventManager {
public :
	typedef std::shared_ptr<EventManager> Ptr;
	typedef std::function<void (Event) > Handler;
	static EventManager::Ptr Create(asio::io_service &);
	~EventManager();

	asio::posix::stream_descriptor & Incoming();
	void Signal(Event e);
	Event NextEvent();
	Event NextEventAsync(Handler & h);

private:
	static std::atomic<bool> s_initialized;
	static EventManager * s_myself;

	int d_pipes[2];
	asio::posix::stream_descriptor d_incoming,d_outgoing;
	struct sigaction d_oldaction;
	static void SigIntHandler(int signal);

	EventManager(asio::io_service & context);



	EventManager(const EventManager &) = delete;
	EventManager & operator=(const EventManager &) = delete;


};


std::ostream & operator<<(std::ostream & out, const Event & e);
