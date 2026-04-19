#pragma once

#include <string>
#include <cstdint>

class NameMapper {
public:
    // ccnx:/ スキームを付加（なければ）
    static std::string addScheme(const std::string& name);

    // ccnx:/ スキームを除去（あれば）
    static std::string removeScheme(const std::string& name);

    // ICSNコンテンツ名にタイムスタンプを付加（ccnx:/ スキーム付きで返す）
    std::string addTimestamp(const std::string& icsn_content_name);

    // タイムスタンプ付き名前からICSNコンテンツ名を抽出（スキームなし）
    std::string removeTimestamp(const std::string& timestamped_name);

private:
    uint64_t getCurrentTimeMs();
};
