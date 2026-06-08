#include "scenes/AssetLocator.h"

#include <array>

namespace sr {

namespace {

template <typename Callback>
std::filesystem::path searchRoots(Callback callback)
{
    std::error_code ec;
    std::filesystem::path root = std::filesystem::current_path(ec);
    if (ec) {
        return {};
    }

    for (int depth = 0; depth <= 3 && !root.empty(); ++depth) {
        if (std::filesystem::path found = callback(root); !found.empty()) {
            return found;
        }
        root = root.parent_path();
    }
    return {};
}

bool isRegularFile(const std::filesystem::path& path)
{
    std::error_code ec;
    return std::filesystem::is_regular_file(path, ec);
}

bool isDirectory(const std::filesystem::path& path)
{
    std::error_code ec;
    return std::filesystem::is_directory(path, ec);
}

} // namespace

std::filesystem::path AssetLocator::findTexture(const std::filesystem::path& fileName)
{
    return searchRoots([&](const std::filesystem::path& root) {
        const std::filesystem::path candidate = root / "res" / "Texture" / fileName;
        return isRegularFile(candidate) ? candidate : std::filesystem::path {};
    });
}

std::filesystem::path AssetLocator::findModel(const std::filesystem::path& fileName)
{
    return searchRoots([&](const std::filesystem::path& root) {
        const std::array directories {
            root / "res" / "Model",
            root / "res" / "Models",
        };
        for (const std::filesystem::path& directory : directories) {
            const std::filesystem::path candidate = directory / fileName;
            if (isRegularFile(candidate)) {
                return candidate;
            }
        }
        return std::filesystem::path {};
    });
}

std::filesystem::path AssetLocator::findFirstModelWithExtension(const std::filesystem::path& extension)
{
    return searchRoots([&](const std::filesystem::path& root) {
        const std::array directories {
            root / "res" / "Model",
            root / "res" / "Models",
        };
        for (const std::filesystem::path& directory : directories) {
            if (!isDirectory(directory)) {
                continue;
            }

            std::error_code ec;
            for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(directory, ec)) {
                if (ec) {
                    break;
                }
                if (entry.is_regular_file(ec) && !ec && entry.path().extension() == extension) {
                    return entry.path();
                }
            }
        }
        return std::filesystem::path {};
    });
}

} // namespace sr
