#include "wtimer.h"

#include <ostream>
#include <sys/time.h>
#include <unistd.h>

using namespace std;

WTimer::WTimer() {
	last_start_clock = current_clock();
	collected_time = 0;
	stopped = false;
}

WTimer::~WTimer() {
}

double WTimer::current_clock() const {
	struct timeval tv;

	if (gettimeofday(&tv, NULL) == -1) {
		return -1.0;
	}

	return (double) tv.tv_sec + (double) tv.tv_usec / 1000000.0;
}

double WTimer::stop() {
	collected_time = (*this)();
	stopped = true;
	return collected_time;
}

double WTimer::operator()() const {
	if (stopped)
		return collected_time;
	else
		return collected_time + current_clock() - last_start_clock;
}

void WTimer::resume() {
	if (stopped) {
		stopped = false;
		last_start_clock = current_clock();
	}
}

double WTimer::reset() {
	double result = (*this)();
	collected_time = 0;
	last_start_clock = current_clock();
	return result;
}

ostream &operator<<(ostream &os, const WTimer &timer) {
	double value = timer();
	if (value < 0 && value > -1e-10)
		value = 0.0; // We sometimes get inaccuracies from god knows where.
	if (value < 1e-10)
		value = 0.0; // Don't care about such small values.
	os << value;
	return os;
}
