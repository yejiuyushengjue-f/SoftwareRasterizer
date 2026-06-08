#pragma once

#include <filesystem>

namespace sr {

class AssetLocator {
public:
    static std::filesystem::path findTexture(const std::filesystem::path& fileName);
    static std::filesystem::path findModel(const std::filesystem::path& fileName);
    static std::filesystem::path findFirstModelWithExtension(const std::filesystem::path& extension);
};

} // namespace sr
