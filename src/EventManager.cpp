#include <fcntl.h>
#include <unistd.h>

#include <glog/logging.h>

#include "EventManager.h"

#include "utils/PosixCall.h"

#include <asio/write.hpp>
#include <asio/read.hpp>
#include <asio/buffer.hpp>


std::atomic<bool> EventManager::s_initialized(false);
EventManager * EventManager::s_myself  = NULL;

EventManager::Ptr EventManager::Create(asio::io_service & service) {
	bool initialized  = false;
	if ( s_initialized.compare_exchange_strong(initialized,true) == false ) {
		throw std::runtime_error("Already initailized");
	}
	EventManager::Ptr res;
	try {
		res = Ptr(new EventManager(service));
	} catch(const std::system_error) {
		s_initialized.store(false);
		throw;
	}

	return res;
}


EventManager::EventManager(asio::io_service & service)
	: d_incoming(service)
	, d_outgoing(service) {
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

	d_incoming = asio::posix::stream_descriptor(service,d_pipes[0]);
	d_outgoing = asio::posix::stream_descriptor(service,d_pipes[1]);
}

void EventManager::SigIntHandler(int signal) {
	if (signal != SIGINT ) {
		LOG(FATAL) << "Only SIGINT should trigger this handler";
	}
	if (s_myself != NULL ) {
		s_myself->Signal(Event::QUIT);
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


asio::posix::stream_descriptor & EventManager::Incoming() {
	return d_incoming;
}

Event EventManager::NextEventAsync(Handler & h) {
	auto e = std::make_shared<Event>();
	d_incoming.async_read_some(asio::buffer(e.get(),sizeof(Event)),[e,h](const asio::error_code& ec,
	                                                                     std::size_t bytes_transferred){
		                           if (!ec && bytes_transferred == sizeof(Event)) {
			                           h(*e);
		                           }
	                           });

}




void EventManager::Signal(Event e) {
	DLOG(INFO) << "Signaling " << e;
	asio::write(d_outgoing,asio::const_buffers_1(&e,sizeof(Event)));
}

Event EventManager::NextEvent() {
	Event newEvent;
	asio::read(d_incoming,asio::mutable_buffers_1(reinterpret_cast<void*>(&newEvent),sizeof(newEvent)));
	return newEvent;
}


std::ostream & operator<<(std::ostream & out, const Event & e) {
	static std::vector<std::string> names = {
		"QUIT",
		"FRAME_READY",
		"PROCESS_NEED_REFRESH",
		"NEW_READOUT",
		"NEW_ANT_DISCOVERED",
	};
	if ( e < Event::NB_EVENTS & (size_t)e >= 0) {
		return out << "Event." << names[(size_t)e];
	}
	return out << "<unknown EVENT>";

}
