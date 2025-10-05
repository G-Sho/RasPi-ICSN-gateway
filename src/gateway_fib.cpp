#include "gateway_fib.h"
#include <sstream>
#include <vector>

GatewayFIB::GatewayFIB(int max_virtual_depth) : maxVirtualDepth_(max_virtual_depth) {}

void GatewayFIB::save(const std::string& content_name, const std::set<std::string>& mac_addresses) {
    FIBEntry entry;
    entry.isVirtual = false;
    entry.maximumDepth = calculateDepth(content_name);
    entry.macAddresses = mac_addresses;

    cache_.put(content_name, entry);
}

std::set<std::string> GatewayFIB::lookup(const std::string& content_name) {
    int nameDepth = calculateDepth(content_name);
    FIBEntry entry;

    if (fibLpmLookup(content_name, nameDepth, maxVirtualDepth_, entry)) {
        return entry.macAddresses;
    }

    return std::set<std::string>();
}

void GatewayFIB::remove(const std::string& content_name) {
    cache_.remove(content_name);
}

bool GatewayFIB::find(const std::string& content_name) {
    return cache_.contains(content_name);
}

std::string GatewayFIB::extractPrefix(const std::string& name, int prefixDepth) const {
    if (prefixDepth <= 0) {
        return "";
    }

    std::vector<std::string> components;
    std::stringstream ss(name);
    std::string component;

    while (std::getline(ss, component, '/')) {
        if (!component.empty()) {
            components.push_back(component);
        }
    }

    if (prefixDepth > components.size()) {
        prefixDepth = components.size();
    }

    std::ostringstream oss;
    for (int i = 0; i < prefixDepth; i++) {
        oss << "/" << components[i];
    }

    return oss.str();
}

bool GatewayFIB::lookupEntry(const std::string& name, int prefixDepth, FIBEntry& outEntry) {
    std::string prefix = extractPrefix(name, prefixDepth);
    return cache_.get(prefix, outEntry);
}

bool GatewayFIB::fibLpmLookup(const std::string& name, int nameDepth, int maxVirtualDepth, FIBEntry& outEntry) {
    // Stage 1: Exact match
    if (lookupEntry(name, nameDepth, outEntry)) {
        return true;
    }

    // Stage 2: Longest prefix match
    for (int depth = nameDepth - 1; depth > 0; depth--) {
        FIBEntry entry;
        if (lookupEntry(name, depth, entry)) {
            if (!entry.isVirtual) {
                outEntry = entry;
                return true;
            }

            // Virtual entry: check maximum depth
            if (nameDepth <= entry.maximumDepth + maxVirtualDepth) {
                outEntry = entry;
                return true;
            }
        }
    }

    return false;
}

int GatewayFIB::calculateDepth(const std::string& name) const {
    if (name.empty() || name == "/") {
        return 0;
    }

    int depth = 0;
    for (char c : name) {
        if (c == '/') {
            depth++;
        }
    }

    // Adjust if name starts with '/'
    if (name[0] == '/') {
        depth--;
    }

    return depth > 0 ? depth : 1;
}
