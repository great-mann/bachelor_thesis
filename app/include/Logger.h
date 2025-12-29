#pragma once

#ifndef BOOST_LOG_AVAILABLE
        #define DEBUG_PRINTF_ENABLED 1
	#define LOG_DEEBUG 1
	#define LOG_WARNING 2
	#include <iostream>
	namespace Logger{
		static const int debug=1;
		static const int warning=2;
	};
	static int logger_debug_enabled = 0;
    //FIX THIS! seems logger_debug_enabled is not alwys set in all classes?!//
    //#define LOG(a) if ((Logger::a==Logger::warning) || logger_debug_enabled) std::cout
    //#define LOG(a) std::cout
    #define LOG(a) if (0) std::cout

#define logger_enable_debug(a){ logger_debug_enabled = (a);}
        //#define BOOST_LOG_TRIVIAL(a) if ((a==loglevel::warning) || logger_debug_enabled) std::cout
#else
        #include <boost/log/core.hpp>
        #include <boost/log/trivial.hpp>
        #include <boost/log/expressions.hpp>
        #include <boost/log/utility/setup/common_attributes.hpp>
        #include <boost/thread/mutex.hpp>
	namespace logging = boost::log;
	#define LOG(a)  BOOST_LOG_TRIVIAL(a)
        #define logger_enable_debug(a){ \
		logging::add_common_attributes();\
		if(a){ \
			logging::core::get()->set_filter( logging::trivial::severity >= logging::trivial::trace );\
		}else{ \
                	logging::core::get()->set_filter( logging::trivial::severity >= logging::trivial::info );\
	        } }

#endif


