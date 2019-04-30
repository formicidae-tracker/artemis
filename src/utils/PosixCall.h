#pragma once


#include <system_error>

#include <artemis-config.h>

#ifdef ARTEMIS_USE_SYSTEM_CATEGORY
#define ARTEMIS_SYSTEM_CATEGORY() std::system_category()
#else
#define ARTEMIS_SYSTEM_CATEGORY() std::generic_category()
#endif


#define ARTEMIS_SYSTEM_ERROR(fnct,err) std::system_error(err,ARTEMIS_SYSTEM_CATEGORY(),std::string("On call of ") + #fnct + "()")


#define p_call(fnct,...) do {	  \
		int artemis_pcall_res ## fnct = fnct(__VA_ARGS__); \
		if ( artemis_pcall_res ## fnct < 0 ) { \
			throw ARTEMIS_SYSTEM_ERROR(fnct,errno); \
		} \
	}while(0)



#define p_call_noerrno(fnct,...) do {	  \
		int artemis_pcall_res ## fnct = fnct(__VA_ARGS__); \
		if ( artemis_pcall_res ## fnct != 0 ) { \
			throw ARTEMIS_SYSTEM_ERROR(fnct,-artemis_pcall_res); \
		} \
	}while(0)
