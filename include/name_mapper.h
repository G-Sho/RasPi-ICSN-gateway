#pragma once

#include <string>

class NameMapper {
public:
    // ccnx:/ スキームを付加（なければ）
    static std::string addScheme(const std::string& name);

    // ccnx:/ スキームを除去（あれば）
    static std::string removeScheme(const std::string& name);
};
