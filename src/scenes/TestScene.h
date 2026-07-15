#pragma once

#include "scenes/LoadDiagnostics.h"
#include "scenes/ScenePreset.h"
#include "renderer/RenderSceneView.h"
#include "renderer/Texture.h"
#include "renderer/Vertex.h"

#include <array>
#include <span>
#include <vector>

namespace sr {

class TestScene {
public:
    TestScene();

    void toggleExhibitRotation();
    void update(float deltaSeconds);
    std::span<const DrawCommand> drawCommands() const { return commands_; }
    RenderSceneView renderView() const;
    const char* activeModelName() const;
    const LoadDiagnostics& loadDiagnostics() const { return diagnostics_; }
    bool exhibitRotationPaused() const { return exhibitRotationPaused_; }
    float exhibitRotationAngle() const { return exhibitRotationAngle_; }

private:
    void applyCentralExhibit();
    void setupStaticCommands();
    void updateCentralTransform();

    enum CommandIndex : std::size_t {
        Floor,
        BackWall,
        LeftWall,
        RightWall,
        Pedestal,
        CentralExhibit,
        AccentBench,
        AccentMonolith,
        CommandCount,
    };

    ScenePreset preset_;
    LoadDiagnostics diagnostics_;
    float exhibitRotationAngle_ = 0.0f;
    bool exhibitRotationPaused_ = false;
    bool usingObjModel_ = false;
    bool objModelAvailable_ = false;
    Texture sculptureTexture_;
    Texture floorTexture_;
    Texture floorNormalTexture_;
    Material centralMaterial_;
    Material pedestalMaterial_;
    Material floorMaterial_;
    Material wallMaterial_;
    Material accentMaterial_;
    Material monolithMaterial_;
    std::vector<Vertex> sculptureMesh_;
    std::vector<Vertex> objMesh_;
    std::vector<Vertex> floorMesh_;
    std::vector<Vertex> wallMesh_;
    std::vector<Vertex> pedestalMesh_;
    std::vector<Vertex> benchMesh_;
    std::vector<Vertex> monolithMesh_;
    std::array<DrawCommand, CommandCount> commands_ {};
};

} // namespace sr
