#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>

#include "libmath/math_operations.h"
#include "libtime/time_operations.h"
#include "libconfig/config_manager.h"

int main() {
    // Math operations example
    int a = 10, b = 5;
    std::cout << "Addition: " << MathOperations::add(a, b) << std::endl;

    // Time operations example
    std::cout << "Current Time: " << TimeOperations::currentTime() << std::endl;

    // Configuration example (load from a hypothetical config file)
    const char* configFile = findConfigFile();
    if (configFile == nullptr) {
        std::cerr << "Error: Configuration file not found." << std::endl;
        exit(1);  // Terminate the program with an error status
    }
    std::cout << "Configuration file: " << configFile << std::endl;

    // Retrieve and print config values using C++ string and iostream
    std::string dbHost = getConfigValue(configFile, "database", "host");
    if (!dbHost.empty()) {
        std::cout << "Database Host: " << dbHost << std::endl;
    } else {
        std::cout << "Key 'host' not found in section 'database'" << std::endl;
    }

    std::string serverPort = getConfigValue(configFile, "server", "port");
    if (!serverPort.empty()) {
        std::cout << "Server Port: " << serverPort << std::endl;
    } else {
        std::cout << "Key 'port' not found in section 'server'" << std::endl;
    }

    std::string dbUser = getConfigValue(configFile, "database", "user");
    if (!dbUser.empty()) {
        std::cout << "Database User: " << dbUser << std::endl;
    } else {
        std::cout << "Key 'user' not found in section 'database'" << std::endl;
    }

    std::string enableLogging = getConfigValue(configFile, "server", "enable_logging");
    if (!enableLogging.empty()) {
        std::cout << "Enable Logging: " << enableLogging << std::endl;
    } else {
        std::cout << "Key 'enable_logging' not found in section 'server'" << std::endl;
    }

    // Normal execution
    std::cout << "Program finished successfully." << std::endl;
    //return 0; // Normal termination
    exit(0);
}
