#ifndef CLOCKCONTROLLER_H
#define CLOCKCONTROLLER_H

#include "datactrl.h"

class ClockController final
{
public:
    void initialize(Clock *clock, int *queueSerial) const;
    void set(Clock *clock, double pts, int serial) const;
    void setAt(Clock *clock, double pts, int serial, double time) const;
    void setSpeed(Clock *clock, double speed) const;
    double value(const Clock *clock) const;
};

#endif // CLOCKCONTROLLER_H
