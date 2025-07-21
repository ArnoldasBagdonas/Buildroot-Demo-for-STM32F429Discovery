// src/libconfig/config_manager.c

#include "libconfig/config_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CONFIG_PATHS_COUNT 3

const char* findConfigFile(void) {
    // Get home directory for user-specific config
    static char userConfigPath[256]; // Ensure the size is sufficient
    const char* homeDir = getenv("HOME");
    if (homeDir != NULL) {
        snprintf(userConfigPath, sizeof(userConfigPath), "%s/.hellomk.ini", homeDir);
    }

    // Array to hold possible config paths
    static const char* possiblePaths[CONFIG_PATHS_COUNT] = {
        "./hellomk.ini",                               // Current directory
        userConfigPath,                                // User-specific (HOME) config
        "/etc/hellomk.ini"                             // System-wide configuration
    };

    for (int i = 0; i < CONFIG_PATHS_COUNT; ++i) {
        if (possiblePaths[i] == NULL) continue; // Skip NULL entries

        FILE* configFile = fopen(possiblePaths[i], "r");
        if (configFile) {
            fclose(configFile);
            return possiblePaths[i]; // Return the first found path
        }
    }

    return NULL; // Return NULL if none found
}

const char* getConfigValue(const char* filename, const char* section, const char* key) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Failed to open config file: %s\n", filename);
        return NULL; // Use NULL to indicate failure
    }

    // Static line buffer to hold lines from the config file
    static char line[LINE_BUFFER_LENGTH];
    
    // Buffer for the current section
    char currentSection[MAX_SECTION_LENGTH] = "";

    while (fgets(line, sizeof(line), file)) {
        // Ignore comments and empty lines
        if (line[0] == '#' || line[0] == '\n') continue;

        // Check for section headers
        if (line[0] == '[') {
            char* endPos = strchr(line, ']');
            if (endPos) {
                *endPos = '\0'; // Null-terminate at the end of section
                strncpy(currentSection, line + 1, MAX_SECTION_LENGTH - 1); // Copy section name
                currentSection[MAX_SECTION_LENGTH - 1] = '\0'; // Ensure null-termination
                // Debug output
                #ifdef DEBUG
                printf("Found section: %s\n", currentSection);
                #endif
                continue;
            }
        }

        // Split the line into key and value
        char* equalsPos = strchr(line, '=');
        if (equalsPos && strlen(currentSection) > 0) {
            *equalsPos = '\0'; // Null-terminate key
            char* currentKey = line;
            char* currentValue = equalsPos + 1;

            // Trim whitespace from key
            while (*currentKey == ' ') currentKey++;
            while (*(currentKey + strlen(currentKey) - 1) == ' ') {
                *(currentKey + strlen(currentKey) - 1) = '\0'; // Null-terminate at last non-space character
            }

            // Trim whitespace from value
            while (*currentValue == ' ') currentValue++;
            currentValue[strcspn(currentValue, "\n")] = 0; // Remove newline

            // Debug output
            #ifdef DEBUG
            printf("Found key-value pair: [%s] %s = %s\n", currentSection, currentKey, currentValue);
            #endif

            // Retrieve the key value 
            if ((strcmp(currentSection, section) == 0) && 
                (strcmp(currentKey, key) == 0)) {
                fclose(file); // Close file before returning
                return currentValue; // Return the found value
            }
        }
    }

    fclose(file); // Close the file when done
    return NULL; // Use NULL to indicate that the key was not found
}
