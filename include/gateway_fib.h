#pragma once

#include <string>
#include <set>
#include "infrastructure/data_access/FixedSizeLRUCache.hpp"

class GatewayFIB {
public:
    GatewayFIB(int max_virtual_depth = 3);

    // Register FIB entry
    void save(const std::string& content_name, const std::set<std::string>& mac_addresses);

    // Longest Prefix Match (LPM with TwoStage algorithm)
    std::set<std::string> lookup(const std::string& content_name);

    // Remove entry
    void remove(const std::string& content_name);

    // Check existence
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
