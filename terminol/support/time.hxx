// vi:noai:sw=4

#ifndef COMMON__TIME__HXX
#define COMMON__TIME__HXX

#include <iosfwd>

#include <stdint.h>

//
// The difference between two points in time, with nanosecond resolution.
//

class TimeDelta {
    int64_t _time;
    explicit TimeDelta(int64_t nanoSeconds) : _time(nanoSeconds) {}
public:
    TimeDelta() : _time(0) {}

    TimeDelta & operator += (TimeDelta t) throw ();
    TimeDelta & operator -= (TimeDelta t) throw ();

    TimeDelta operator - () const throw ();
};

std::ostream & operator << (std::ostream & ost, TimeDelta td);

TimeDelta operator + (TimeDelta lhs, TimeDelta rhs);
TimeDelta operator - (TimeDelta lhs, TimeDelta rhs);
TimeDelta operator * (double    lhs, TimeDelta rhs);
TimeDelta operator / (TimeDelta lhs, double    rhs);
double    operator / (TimeDelta lhs, TimeDelta rhs);

bool operator == (TimeDelta lhs, TimeDelta rhs);
bool operator != (TimeDelta lhs, TimeDelta rhs);
bool operator <  (TimeDelta lhs, TimeDelta rhs);
bool operator <= (TimeDelta lhs, TimeDelta rhs);
bool operator >  (TimeDelta lhs, TimeDelta rhs);
bool operator >= (TimeDelta lhs, TimeDelta rhs);

//
// A point in time represented as nanoseconds since 1970 started.
//

class TimeStamp {
    int64_t _time;
    explicit TimeStamp(int64_t nanoSeconds) : _time(nanoSeconds) {}
public:
    TimeStamp() : _time(0) {}

    TimeStamp & operator += (TimeDelta t) throw ();
    TimeStamp & operator -= (TimeDelta t) throw ();
};

std::ostream & operator << (std::ostream & ost, TimeStamp ts);

TimeStamp operator + (TimeStamp lhs, TimeDelta rhs);
TimeStamp operator - (TimeStamp lhs, TimeDelta rhs);
TimeDelta operator - (TimeStamp lhs, TimeStamp rhs);

bool operator == (TimeStamp lhs, TimeStamp rhs);
bool operator != (TimeStamp lhs, TimeStamp rhs);
bool operator <  (TimeStamp lhs, TimeStamp rhs);
bool operator <= (TimeStamp lhs, TimeStamp rhs);
bool operator >  (TimeStamp lhs, TimeStamp rhs);
bool operator >= (TimeStamp lhs, TimeStamp rhs);

//
// A timer that expires.
//

class Timer {
    uint32_t _sec;
    uint32_t _usec;

public:
    Timer(uint32_t milliseconds);
    bool expired() const;
};

#endif // COMMON__TIME__HXX
