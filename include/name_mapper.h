#pragma once

#include <string>

class NameMapper {
public:
    // Add timestamp to ICSN content name
    std::string addTimestamp(const std::string& icsn_content_name);

    // Extract ICSN content name from timestamped name
    std::string removeTimestamp(const std::string& timestamped_name);

private:
    uint64_t getCurrentTimeMs();
};
