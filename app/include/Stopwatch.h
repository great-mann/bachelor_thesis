#ifndef __Stopwatch_h_included__
#define __Stopwatch_h_included__

#include <string>
#include <map>

struct TimeKeeper {
	std::string name;
	double start;
	double end;
	double duration;
	double total_duration;
	long measurements;
	double mean_duration;

	TimeKeeper() {
		start = 0;
		end = 0;
		duration = 0;
		measurements = 0;
		mean_duration = 0;
		total_duration = 0;
		name = "";
	};
};

class Stopwatch {
public:
	Stopwatch();
	void start( std::string );
	double stop( std::string );
	double getDuration( std::string );
	double getMeanDuration( std::string );

	void print( std::ostream& );

	static double getTimeInMilliseconds();

private:
	std::map<std::string, TimeKeeper> entries;
};

class StopwatchGuard {
public:
	StopwatchGuard( Stopwatch& watch, std::string name ) : mWatch(watch), mName(name) {
		mWatch.start(mName);
	};

	void start() {
		mWatch.start(mName);
	}

	~StopwatchGuard() {
		mWatch.stop(mName);
	}
private:
	Stopwatch& mWatch;
	std::string mName;
};

#endif // __Stopwatch_h_included__
