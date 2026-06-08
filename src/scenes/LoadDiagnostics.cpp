#include "scenes/LoadDiagnostics.h"

#include <utility>

namespace sr {

void LoadDiagnostics::recordTextureFailure(std::string target, std::string reason)
{
    entries_.push_back({ Kind::Texture, std::move(target), std::move(reason) });
}

void LoadDiagnostics::recordObjFailure(std::string target, std::string reason)
{
    entries_.push_back({ Kind::Obj, std::move(target), std::move(reason) });
}

} // namespace sr
