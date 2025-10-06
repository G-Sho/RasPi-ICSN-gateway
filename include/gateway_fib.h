#pragma once

#include <string>
#include <set>
#include "infrastructure/data_access/FixedSizeLRUCache.hpp"

class GatewayFIB {
public:
    GatewayFIB(int max_virtual_depth = 3);

    // FIBエントリ登録
    void save(const std::string& content_name, const std::set<std::string>& mac_addresses);

    // 最長一致検索（TwoStageアルゴリズムによるLPM）
    std::set<std::string> lookup(const std::string& content_name);

    // エントリ削除
    void remove(const std::string& content_name);

    // 存在確認
    bool find(const std::string& content_name);

private:
    struct FIBEntry {
        bool isVirtual;
        int maximumDepth;
        std::set<std::string> macAddresses;

        FIBEntry() : isVirtual(false), maximumDepth(0) {}
    };

    FixedSizeLRUCache<FIBEntry, 100> cache_;
    int maxVirtualDepth_;

    std::string extractPrefix(const std::string& name, int prefixDepth) const;
    bool lookupEntry(const std::string& name, int prefixDepth, FIBEntry& outEntry);
    bool fibLpmLookup(const std::string& name, int nameDepth, int maxVirtualDepth, FIBEntry& outEntry);
    int calculateDepth(const std::string& name) const;
};
