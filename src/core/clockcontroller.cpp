#include "clockcontroller.h"

#include <cmath>

void ClockController::initialize(Clock *clock, int *queueSerial) const
{
    clock->speed = 1.0;
    clock->paused = 0;
    clock->queue_serial = queueSerial;
    set(clock, NAN, -1);
}

void ClockController::set(Clock *clock, double pts, int serial) const
{
    setAt(clock, pts, serial, av_gettime_relative() / 1000000.0);
}

void ClockController::setAt(Clock *clock, double pts, int serial, double time) const
{
    clock->pts = pts;
    clock->last_updated = time;
    clock->pts_drift = pts - time;
    clock->serial = serial;
}

void ClockController::setSpeed(Clock *clock, double speed) const
{
    set(clock, value(clock), clock->serial);
    clock->speed = speed;
}

double ClockController::value(const Clock *clock) const
{
    if (!clock->queue_serial || *clock->queue_serial != clock->serial) {
        return NAN;
    }
    if (clock->paused) {
        return clock->pts;
    }

    const double time = av_gettime_relative() / 1000000.0;
    return clock->pts_drift + time
        - (time - clock->last_updated) * (1.0 - clock->speed);
}
