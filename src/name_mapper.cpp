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

    // コンテンツ名が'/'で始まることを保証
    if (icsn_content_name.empty() || icsn_content_name[0] != '/') {
        oss << "/" << icsn_content_name;
    } else {
        oss << icsn_content_name;
    }

    // 末尾の'/'があれば削除
    std::string name = oss.str();
    if (name.length() > 1 && name.back() == '/') {
        name.pop_back();
    }

    // タイムスタンプ付加
    oss.str("");
    oss << name << "/" << timestamp;

    return oss.str();
}

std::string NameMapper::removeTimestamp(const std::string& timestamped_name) {
    // 最後の'/'を検索
    size_t last_slash = timestamped_name.rfind('/');

    if (last_slash == std::string::npos || last_slash == 0) {
        return timestamped_name;
    }

    // コンテンツ名を抽出（タイムスタンプなし）
    return timestamped_name.substr(0, last_slash);
}
