#include "name_mapper.h"

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
