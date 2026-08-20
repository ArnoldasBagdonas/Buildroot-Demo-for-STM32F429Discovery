// include/libtime/time_operations.h

#ifndef TIME_OPERATIONS_H
#define TIME_OPERATIONS_H

#include <string>
#include <chrono>

class TimeOperations {
public:
    static std::string currentTime();
    static void sleepForSeconds(int seconds);
};

#endif // TIME_OPERATIONS_H
