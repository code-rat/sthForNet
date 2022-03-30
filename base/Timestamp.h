#pragma once 

#include <stdint.h>
#include <algorithm>
#include <string>

using namespace std;

class Timestamp{
public:
    Timestamp():microSecondsSinceEpoch_(0)
    {
    }

    explicit Timestamp(int64_t microSecondsSinceEpoch);

    Timestamp& operator+=(Timestamp lhs)
    {
        this->microSecondsSinceEpoch_+=lhs.microSecondsSinceEpoch_;
        return *this;
    }

    Timestamp& operator+=(int64_t lhs)
    {
        this->microSecondsSinceEpoch_+=lhs;
        return *this;
    }

    Timestamp& operator-=(Timestamp lhs)
    {
        this->microSecondsSinceEpoch_-=lhs.microSecondsSinceEpoch_;
        return *this;
    }

    Timestamp& operator-=(int64_t lhs)
    {
        this->microSecondsSinceEpoch_-=lhs;
        return *this;
    }

    void swap(Timestamp& that)
    {
        std::swap(microSecondsSinceEpoch_,that.microSecondsSinceEpoch_);
    }

    string toString() const;
    string toFormattedString(bool showMicroseconds=true) const;

    bool vaild() const{return microSecondsSinceEpoch_>0;}

    int64_t  microSecondsSinceEpoch(){return microSecondsSinceEpoch_;}
    time_t secondsSinceEpoch()const
    {
        return static_cast<time_t>(microSecondsSinceEpoch_/kMicroSecondsPerSecond);
    }

    static Timestamp now();
    static Timestamp invaild();
    static const int kMicroSecondsPerSecond=1000*1000;
private:
    int64_t microSecondsSinceEpoch_;
};

inline bool operator<(Timestamp lhs,Timestamp rhs)
{
    return lhs.microSecondsSinceEpoch()<rhs.microSecondsSinceEpoch();
}

inline bool operator>(Timestamp lhs,Timestamp rhs)
{
    return rhs<lhs;
}

inline bool operator<=(Timestamp  lhs,Timestamp rhs)
{
    return !(lhs>rhs);
}

inline bool operator>=(Timestamp lhs,Timestamp rhs)
{
    return !(lhs<rhs);
}

inline bool operator==(Timestamp lhs,Timestamp rhs)
{
    return lhs.microSecondsSinceEpoch()==rhs.microSecondsSinceEpoch();
}

inline bool operator!=(Timestamp lhs,Timestamp rhs)
{
    return !(lhs==rhs);
}

///获取两个时间戳的差值  结果是秒(result in second )
///@param high,low
///@return high-low in seconds
///double   返回    精度对微秒来说足够
inline double timeDifference(Timestamp high,Timestamp low)
{
    int64_t diff=high.microSecondsSinceEpoch()-low.microSecondsSinceEpoch();
    return static_cast<double>(diff/Timestamp::kMicroSecondsPerSecond);
}

inline Timestamp addTime(Timestamp timestamp,int64_t microseconds)
{
    return Timestamp(timestamp.microSecondsSinceEpoch()+microseconds);
}
