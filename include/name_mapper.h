#pragma once

#include <string>

class NameMapper {
public:
    // ccnx:/ スキームを付加（なければ）
    static std::string addScheme(const std::string& name);

    // ccnx:/ スキームを除去（あれば）
    static std::string removeScheme(const std::string& name);

    // CEFOREが付加したTLV形式のコンポーネント（"0x"で始まるパスセグメント）を末尾から除去
    static std::string stripCeforeComponents(const std::string& name);
};
