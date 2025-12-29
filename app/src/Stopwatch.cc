#include <opencv2/opencv.hpp>

#include "Stopwatch.h"

Stopwatch
::Stopwatch() {
}

void 
Stopwatch
::start( std::string name ) {
	TimeKeeper tk;
	if( entries.find( name ) != entries.end() )
		tk = entries.find(name)->second;
	else
		tk.name = name;
	tk.start = getTimeInMilliseconds();
	entries[name] = tk;
}

double 
Stopwatch
::stop( std::string name ) {
	if( entries.find( name ) != entries.end() )
	{
		TimeKeeper tk;
		tk = entries.find(name)->second;
		tk.end = getTimeInMilliseconds();
		tk.duration = tk.end - tk.start;
		tk.total_duration += tk.duration;
		tk.measurements++;
		tk.mean_duration = tk.total_duration / tk.measurements;
		entries[name] = tk;
		return tk.duration;
	} else 
		return -1;
}

double 
Stopwatch
::getDuration( std::string name ) {
	if( entries.find( name ) != entries.end() )
	{
		TimeKeeper tk;
		tk = entries.find(name)->second;
		return tk.duration;
	} else 
		return -1;
}

double 
Stopwatch
::getMeanDuration( std::string name) {
	if( entries.find( name ) != entries.end() )
	{
		TimeKeeper tk;
		tk = entries.find(name)->second;
		return tk.mean_duration;
	} else 
		return -1;}


void 
Stopwatch
::print( std::ostream& out ) {
	std::map<std::string, TimeKeeper>::const_iterator it = entries.begin();
	while( it != entries.end() ) {
		out << it->second.name << "[" << it->second.measurements << "]: duration=" << it->second.duration << " mean=" << it->second.mean_duration << "\n";
		++it;
	}
}

double Stopwatch::getTimeInMilliseconds() {
	return (cv::getTickCount() / cv::getTickFrequency()) * 1000;
};
