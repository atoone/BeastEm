#include "assets.hpp"
#include <cstdlib>

static std::string assetBasePath;
static bool initialized = false;

void initAssetPath(const std::string& cliPath) {
    if (!cliPath.empty()) {
        assetBasePath = cliPath;
    } else {
        const char* envPath = std::getenv("BEASTEM_ASSETS");
        if (envPath != nullptr && envPath[0] != '\0') {
            assetBasePath = envPath;
        } else {
            assetBasePath = "";
        }
    }

    // Ensure path ends with separator if non-empty
    if (!assetBasePath.empty() && assetBasePath.back() != '/') {
        assetBasePath += '/';
    }

    initialized = true;
}

std::string assetPath(const char* filename) {
    if (!initialized) {
        initAssetPath();
    }
    return assetBasePath + filename;
}

std::string assetPath(const std::string& filename) {
    return assetPath(filename.c_str());
}
