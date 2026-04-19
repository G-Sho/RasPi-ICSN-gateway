#include "name_mapper.h"
#include <chrono>
#include <sstream>

static const std::string kScheme = "ccnx:";

std::string NameMapper::addScheme(const std::string& name) {
    if (name.compare(0, kScheme.size(), kScheme) == 0) {
        return name;
    }
    if (!name.empty() && name[0] == '/') {
        return kScheme + name;
    }
    return kScheme + "/" + name;
}

std::string NameMapper::removeScheme(const std::string& name) {
    if (name.compare(0, kScheme.size(), kScheme) != 0) {
        return name;
    }
    std::string result = name.substr(kScheme.size());
    if (result.empty() || result[0] != '/') {
        result = "/" + result;
    }
    return result;
}

uint64_t NameMapper::getCurrentTimeMs() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch());
    return ms.count();
}

std::string NameMapper::addTimestamp(const std::string& icsn_content_name) {
    // スキームなしの正規パスに正規化
    std::string name = removeScheme(icsn_content_name);

    // '/' で始まることを保証
    if (name.empty() || name[0] != '/') {
        name = "/" + name;
    }

    // 末尾の '/' を除去
    if (name.length() > 1 && name.back() == '/') {
        name.pop_back();
    }

    uint64_t timestamp = getCurrentTimeMs();
    std::ostringstream oss;
    oss << name << "/" << timestamp;

    // ccnx:/ スキーム付きで返す（CEFORE API が要求するフォーマット）
    return addScheme(oss.str());
}

std::string NameMapper::removeTimestamp(const std::string& timestamped_name) {
    size_t last_slash = timestamped_name.rfind('/');

    if (last_slash == std::string::npos || last_slash == 0) {
        return timestamped_name;
    }

    return removeScheme(timestamped_name.substr(0, last_slash));
}
