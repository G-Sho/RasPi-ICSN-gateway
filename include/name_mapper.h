#pragma once

#include <string>
#include <cstdint>

class NameMapper {
public:
    // ICSNコンテンツ名にタイムスタンプを付加
    std::string addTimestamp(const std::string& icsn_content_name);

    // タイムスタンプ付き名前からICSNコンテンツ名を抽出
    std::string removeTimestamp(const std::string& timestamped_name);

private:
    uint64_t getCurrentTimeMs();
};
