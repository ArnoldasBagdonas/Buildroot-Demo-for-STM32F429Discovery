// include/libconfig/config_manager.h
#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

// Constants for maximum lengths
#define MAX_KEY_LENGTH 50
#define MAX_VALUE_LENGTH 100
#define MAX_SECTION_LENGTH 50

// Maximum length for the line buffer in the config file
#define LINE_BUFFER_LENGTH 256

// Function declarations for configuration management
const char* findConfigFile(void);
const char* getConfigValue(const char* filename, const char* section, const char* key);

#ifdef __cplusplus
}
#endif

#endif // CONFIG_MANAGER_H
