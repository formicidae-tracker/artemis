#include <fcntl.h>
#include <unistd.h>

#include <glog/logging.h>

#include "EventManager.h"

#include "utils/PosixCall.h"


std::atomic<bool> EventManager::s_initialized(false);
EventManager * EventManager::s_myself  = NULL;

EventManager::Ptr EventManager::Create() {
	bool initialized  = false;
	if ( s_initialized.compare_exchange_strong(initialized,true) == false ) {
		throw std::runtime_error("Already initailized");
	}
	EventManager::Ptr res;
	try {
		res = Ptr(new EventManager());
	} catch(const std::system_error) {
		s_initialized.store(false);
		throw;
	}

	return res;
}


EventManager::EventManager() {
	d_pipes[0] = 0;
	d_pipes[1] = 1;
	p_call(pipe2,d_pipes,O_CLOEXEC | O_NONBLOCK);

	struct sigaction action;

	action.sa_sigaction = NULL;
	action.sa_flags = (d_oldaction.sa_flags | SA_RESTART ) & ~(SA_SIGINFO);
	action.sa_handler = &SigIntHandler;

	s_myself = this;
	try {
		p_call(sigaction,SIGINT,&action,&d_oldaction);
	} catch (const std::exception & e) {
		s_myself = NULL;
		throw;
	}

}

void EventManager::SigIntHandler(int signal) {
	if (signal != SIGINT ) {
		LOG(FATAL) << "Only SIGINT should trigger this handler";
	}
	if (s_myself != NULL ) {
		s_myself->Signal(QUIT);
	} else {
		LOG(FATAL) << "Unconsistent state";
	}
}

EventManager::~EventManager() {
	for (size_t i = 0; i <2; ++i) {
		if (d_pipes[i] > 0 ) {
			close(d_pipes[i]);
		}
	}

	try {
		p_call(sigaction,SIGINT,&d_oldaction,NULL);
	} catch (const std::exception &) {
		LOG(FATAL) << "Could not uninstall SIGINT Handler";
	}
	s_myself = NULL;
	s_initialized.store(false);
}

int EventManager::FileDescriptor() const {
	return d_pipes[0];
}

void EventManager::Signal(Event e) const {
	DLOG(INFO) << "Signaling " << e;
	p_call(write,d_pipes[1],&e,sizeof(Event));
}

EventManager::Event EventManager::NextEvent() const {
	Event newEvent;
	try {
		p_call(read,d_pipes[0],&newEvent,sizeof(Event));
	} catch (std::system_error & e) {
		if ( e.code() == std::errc::resource_unavailable_try_again || e.code() == std::errc::operation_would_block ) {
			return Event::NONE;
		}
		throw;
	}
	return newEvent;
}


std::ostream & operator<<(std::ostream & out, const EventManager::Event & e) {
	static std::vector<std::string> names = {
		"NONE",
		"QUIT",
		"FRAME_READY",
		"PROCESS_NEED_REFRESH",
	};
	if ( e < EventManager::NB_EVENTS & e >= 0) {
		return out << "Event." << names[(size_t)e];
	}
	return out << "<unknown EVENT>";

}
