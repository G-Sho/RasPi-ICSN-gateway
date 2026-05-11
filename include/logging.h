#pragma once

#include <iostream>

#define ICSN_LOG_LEVEL_WARN 1
#define ICSN_LOG_LEVEL_INFO 2
#define ICSN_LOG_LEVEL_DEBUG 3

#ifndef ICSN_LOG_LEVEL
#error "ICSN_LOG_LEVEL must be defined by CMake BUILD_PROFILE (normal|perf|release)"
#endif

#define LOG_ERROR(message) do { std::cerr << "[ERROR] " << message << std::endl; } while(0) // ERRORは全プロファイルで常時表示
#define LOG_WARN(message) do { if (ICSN_LOG_LEVEL >= ICSN_LOG_LEVEL_WARN) { std::cerr << "[WARN] " << message << std::endl; } } while(0)
#define LOG_INFO(message) do { if (ICSN_LOG_LEVEL >= ICSN_LOG_LEVEL_INFO) { std::cout << "[INFO] " << message << std::endl; } } while(0)
#define LOG_DEBUG(message) do { if (ICSN_LOG_LEVEL >= ICSN_LOG_LEVEL_DEBUG) { std::cout << "[DEBUG] " << message << std::endl; } } while(0)
