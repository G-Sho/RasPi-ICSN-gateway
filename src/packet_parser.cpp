#include "packet_parser.h"
#include <cstring>

// CommunicationData structure from ESP32 (packed)
struct __attribute__((packed)) CommunicationData {
    char signalCode[10];
    uint8_t hopCount;
    char contentName[100];
    char content[20];
};

bool PacketParser::parse(const std::vector<uint8_t>& raw_data, SensorData& output) {
    if (raw_data.size() != sizeof(CommunicationData)) {
        return false;
    }

    CommunicationData comm_data;
    memcpy(&comm_data, raw_data.data(), sizeof(CommunicationData));

    // Copy to output
    strncpy(output.signal_code, comm_data.signalCode, 10);
    output.signal_code[9] = '\0';

    output.hop_count = comm_data.hopCount;

    strncpy(output.content_name, comm_data.contentName, 100);
    output.content_name[99] = '\0';

    strncpy(output.content, comm_data.content, 20);
    output.content[19] = '\0';

    return true;
}
