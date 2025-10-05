#include "name_mapper.h"
#include <chrono>
#include <sstream>

uint64_t NameMapper::getCurrentTimeMs() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch());
    return ms.count();
}

std::string NameMapper::addTimestamp(const std::string& icsn_content_name) {
    uint64_t timestamp = getCurrentTimeMs();
    std::ostringstream oss;

    // Ensure the content name starts with '/'
    if (icsn_content_name.empty() || icsn_content_name[0] != '/') {
        oss << "/" << icsn_content_name;
    } else {
        oss << icsn_content_name;
    }

    // Remove trailing '/' if exists
    std::string name = oss.str();
    if (name.length() > 1 && name.back() == '/') {
        name.pop_back();
    }

    // Add timestamp
    oss.str("");
    oss << name << "/" << timestamp;

    return oss.str();
}

std::string NameMapper::removeTimestamp(const std::string& timestamped_name) {
    // Find last '/'
    size_t last_slash = timestamped_name.rfind('/');

    if (last_slash == std::string::npos || last_slash == 0) {
        return timestamped_name;
    }

    // Extract content name (without timestamp)
    return timestamped_name.substr(0, last_slash);
}
