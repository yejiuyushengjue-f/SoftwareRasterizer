#pragma once

#include <string>
#include <vector>

namespace sr {

class LoadDiagnostics {
public:
    enum class Kind {
        Texture,
        Obj,
    };

    struct Entry {
        Kind kind;
        std::string target;
        std::string reason;
    };

    void recordTextureFailure(std::string target, std::string reason);
    void recordObjFailure(std::string target, std::string reason);
    const std::vector<Entry>& entries() const { return entries_; }
    bool empty() const { return entries_.empty(); }

private:
    std::vector<Entry> entries_;
};

} // namespace sr
