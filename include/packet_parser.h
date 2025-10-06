#pragma once

#include <cstdint>
#include <vector>

class PacketParser {
public:
    struct SensorData {
        char signal_code[10];      // シグナルコード ("DATA" または "INTEREST")
        uint8_t hop_count;         // ホップカウント
        char content_name[100];    // コンテンツ名
        char content[20];          // コンテンツ本体
    };

    bool parse(const std::vector<uint8_t>& raw_data, SensorData& output);
};
