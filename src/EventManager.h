#pragma once

#include <signal.h>

#include <memory>
#include <atomic>
#include <iostream>




class EventManager {
public :
	typedef std::shared_ptr<EventManager> Ptr;

	enum Event {
		NONE = 0,
		QUIT = 1,
		FRAME_READY  = 2,
		PROCESS_FINISHED = 3,
		NB_EVENTS,
	};
	static EventManager::Ptr Create();
	~EventManager();

	int FileDescriptor() const;
	void Signal(Event e) const;
	Event NextEvent() const;


private:
	static std::atomic<bool> s_initialized;
	static EventManager * s_myself;
	int d_pipes[2];
	struct sigaction d_oldaction;
	static void SigIntHandler(int signal);

	EventManager();



	EventManager(const EventManager &) = delete;
	EventManager & operator=(const EventManager &) = delete;

};


std::ostream & operator<<(std::ostream & out, const EventManager::Event & e);
