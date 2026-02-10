#pragma once
#include <string>

// Initialize the asset path from (in order):
// 1. The provided path argument (from -A CLI option), if non-empty
// 2. BEASTEM_ASSETS environment variable, if set
// 3. Current working directory (empty string, meaning relative paths)
void initAssetPath(const std::string& cliPath = "");

// Returns the full path to an asset file
std::string assetPath(const char* filename);
std::string assetPath(const std::string& filename);
