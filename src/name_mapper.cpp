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

std::string NameMapper::stripCeforeComponents(const std::string& name) {
    std::string result = name;
    // CEFOREが付加したTLV形式コンポーネント（例: /0x0004=0x00000000）を末尾から除去する
    while (true) {
        size_t last_slash = result.rfind('/');
        if (last_slash == std::string::npos || last_slash == 0) {
            break;
        }
        std::string segment = result.substr(last_slash + 1);
        if (segment.compare(0, 2, "0x") == 0 || segment.compare(0, 2, "0X") == 0) {
            result = result.substr(0, last_slash);
        } else {
            break;
        }
    }
    return result;
}
