// src/libtime/time_operations.c

#include "libtime/time_operations.h"
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <unistd.h> // For sleep()

// Function to get the current time as a string
const char* currentTime() {
    static char buffer[20]; // Buffer to hold the time string in format "YYYY-MM-DD HH:MM:SS"
    time_t now = time(NULL);
    struct tm *localTime = localtime(&now);

    // Format the time as a string
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", localTime);
    return buffer;
}

// Function to sleep for a specified number of seconds
void sleepForSeconds(int seconds) {
    sleep(seconds); // Use sleep from unistd.h
}
