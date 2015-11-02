#ifndef WTIMER_H
#define WTIMER_H

#include <iosfwd>

class WTimer {
    double last_start_clock;
    double collected_time;
    bool stopped;

    double current_clock() const;
public:
    WTimer();
    ~WTimer();
    double operator()() const;
    double stop();
    void resume();
    double reset();
};

std::ostream &operator<<(std::ostream &os, const WTimer &timer);

#endif
