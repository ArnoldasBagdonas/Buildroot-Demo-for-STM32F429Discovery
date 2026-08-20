// src/main.c

#include "libmath/math_operations.h"
#include "libtime/time_operations.h"
#include "libconfig/config_manager.h"

#include <stdio.h>


int main() {
    // Math operations example
    int a = 10, b = 5;
    printf("Addition: %d\n", add(a, b)); // Assuming add is a function in libmath

    // Time operations example
    printf("Current Time: %s\n", currentTime()); // Assuming currentTime returns a string

    // Configuration example (load from a hypothetical config file)
    const char* configFile = findConfigFile();
    if (configFile == NULL) {
        fprintf(stderr, "Configuration file not found.\n");
        return 1; // Error
    }
    printf("Configuration file: %s\n", configFile);

    // Load configuration from the found path
    const char* dbHost = getConfigValue(configFile, "database", "host");
    if (dbHost) {
        printf("Database Host: %s\n", dbHost);
    } else {
        printf("Key not found in section 'database'\n");
    }

    const char* serverPort = getConfigValue(configFile, "server", "port");
    if (serverPort) {
        printf("Server Port: %s\n", serverPort);
    } else {
        printf("Key not found in section 'server'\n");
    }

    const char* dbUser = getConfigValue(configFile, "database", "user");
    if (dbUser) {
        printf("Database User: %s\n", dbUser);
    } else {
        printf("Key not found in section 'database'\n");
    }

    const char* enableLogging = getConfigValue(configFile, "server", "enable_logging");
    if (enableLogging) {
        printf("Enable Logging: %s\n", enableLogging);
    } else {
        printf("Key not found in section 'server'\n");
    }

    return 0;
}
