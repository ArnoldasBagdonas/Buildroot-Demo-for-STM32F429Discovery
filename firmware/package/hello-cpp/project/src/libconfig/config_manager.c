#include "libconfig/config_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CONFIG_PATHS_COUNT 3

const char* findConfigFile(void) {
    // Get home directory for user-specific config
    static char userConfigPath[256];  // Buffer for user-specific config path
    const char* homeDir = getenv("HOME");
    
    if (homeDir != NULL) {
        snprintf(userConfigPath, sizeof(userConfigPath), "%s/%s", homeDir, CONFIG_FILE);
    }

    // Array to hold possible config paths
    static const char* possiblePaths[CONFIG_PATHS_COUNT] = {
        CONFIG_FILE,                 // Current directory
        userConfigPath,              // User-specific (HOME) config
        "/etc/" CONFIG_FILE          // System-wide configuration
    };

    // Search for the config file in possible locations
    for (int i = 0; i < CONFIG_PATHS_COUNT; ++i) {
        if (possiblePaths[i] == NULL) continue; // Skip NULL entries

        // Debug output: Attempting to open the config file
        #ifdef DEBUG
        printf("Trying config file path: %s\n", possiblePaths[i]);
        #endif

        FILE* configFile = fopen(possiblePaths[i], "r");
        if (configFile) {
            fclose(configFile);

            // Debug output: Successfully found the config file
            #ifdef DEBUG
            printf("Config file found: %s\n", possiblePaths[i]);
            #endif

            return possiblePaths[i]; // Return the first found path
        }
    }

    // Debug output: Config file not found
    #ifdef DEBUG
    printf("Config file not found in any path.\n");
    #endif

    return NULL; // Return NULL if none found
}

const char* getConfigValue(const char* filename, const char* section, const char* key) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        #ifdef DEBUG
        printf("Failed to open config file: %s\n", filename);
        #endif
        return NULL; // Return NULL on failure
    }

    // Static line buffer to hold lines from the config file
    static char line[LINE_BUFFER_LENGTH];

    // Buffers for current section and key-value pairs
    char currentSection[MAX_SECTION_LENGTH] = "";

    // Search for the key-value pair in the config file
    while (fgets(line, sizeof(line), file)) {
        // Ignore comments and empty lines
        if (line[0] == '#' || line[0] == '\n') continue;

        // Check for section headers
        if (line[0] == '[') {
            char* endPos = strchr(line, ']');
            if (endPos) {
                *endPos = '\0';  // Null-terminate at the closing bracket ']'
                strncpy(currentSection, line + 1, MAX_SECTION_LENGTH - 1); // Copy section name
                currentSection[MAX_SECTION_LENGTH - 1] = '\0'; // Ensure null-termination

                // Debug output: Section found
                #ifdef DEBUG
                printf("Found section: %s\n", currentSection);
                #endif
                continue;
            }
        }

        // Split the line into key and value
        char* equalsPos = strchr(line, '=');
        if (equalsPos && strlen(currentSection) > 0) {
            *equalsPos = '\0';  // Null-terminate key

            char* currentKey = line;
            char* currentValue = equalsPos + 1;

            // Trim leading and trailing spaces from key
            while (*currentKey == ' ') currentKey++;
            while (*(currentKey + strlen(currentKey) - 1) == ' ') {
                *(currentKey + strlen(currentKey) - 1) = '\0'; // Null-terminate
            }

            // Trim leading and trailing spaces from value
            while (*currentValue == ' ') currentValue++;
            currentValue[strcspn(currentValue, "\n")] = '\0'; // Remove newline character

            // Debug output: Key-value pair found
            #ifdef DEBUG
            printf("Found key-value pair: [%s] %s = %s\n", currentSection, currentKey, currentValue);
            #endif

            // Check if the current section and key match the requested ones
            if ((strcmp(currentSection, section) == 0) && (strcmp(currentKey, key) == 0)) {
                fclose(file);  // Close the file before returning

                // Return a copy of the value to avoid returning a pointer to static buffer
                static char returnValue[MAX_VALUE_LENGTH];
                strncpy(returnValue, currentValue, MAX_VALUE_LENGTH - 1);
                returnValue[MAX_VALUE_LENGTH - 1] = '\0'; // Ensure null-termination

                return returnValue; // Return the found value
            }
        }
    }

    // Key not found in the file
    #ifdef DEBUG
    printf("Key '%s' not found in section '%s'.\n", key, section);
    #endif

    fclose(file); // Close the file when done
    return NULL;  // Return NULL if the key is not found
}
