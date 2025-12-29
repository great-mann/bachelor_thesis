#include "DebugHelpers.h"

std::stringstream InstantIOLogger::logStringStream;

void InstantIOLogger::addLog(std::string s) {
	logStringStream << s << "\n";
}

std::string InstantIOLogger::flushLogStr() {
	std::string out = logStringStream.str();
	logStringStream.str("");
	logStringStream.clear();
	return out;
}




