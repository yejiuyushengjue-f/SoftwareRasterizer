#pragma once

#include "renderer/Material.h"
#include "renderer/Texture.h"
#include "renderer/Vertex.h"

#include <array>
#include <span>
#include <vector>

namespace sr {

class TestScene {
public:
    TestScene();

    void toggleModel();
    void update(float deltaSeconds);
    std::span<const DrawCommand> drawCommands() const { return commands_; }
    const char* activeModelName() const;

private:
    void applyActiveModel();

    float rotation_ = 0.0f;
    bool usingObjModel_ = false;
    bool objModelAvailable_ = false;
    Texture modelTexture_;
    Texture cubeTexture_;
    Texture normalTexture_;
    Material modelMaterial_;
    Material cubeMaterial_;
    Material groundMaterial_;
    std::vector<Vertex> sphereMesh_;
    std::vector<Vertex> objMesh_;
    std::vector<Vertex> cubeMesh_;
    std::vector<Vertex> groundMesh_;
    std::array<DrawCommand, 3> commands_;
};

} // namespace sr
