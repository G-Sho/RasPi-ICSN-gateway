#pragma once

#include <cstdint>
#include <vector>

class PacketParser {
public:
    struct SensorData {
        char signal_code[10];      // Signal code ("DATA" or "INTEREST")
        uint8_t hop_count;         // Hop count
        char content_name[100];    // Content name
        char content[20];          // Content body
    };

    bool parse(const std::vector<uint8_t>& raw_data, SensorData& output);
};
