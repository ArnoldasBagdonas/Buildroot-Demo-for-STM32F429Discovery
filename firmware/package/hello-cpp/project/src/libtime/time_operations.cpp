// src/libtime/time_operations.cpp

#include "libtime/time_operations.h"
#include <chrono>
#include <thread>
#include <ctime>
#include <iomanip>
#include <sstream>

std::string TimeOperations::currentTime() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_c), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

void TimeOperations::sleepForSeconds(int seconds) {
    std::this_thread::sleep_for(std::chrono::seconds(seconds));
}
