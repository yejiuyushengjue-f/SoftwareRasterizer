#include "scenes/TestScene.h"

#include "scenes/AssetLocator.h"
#include "scenes/MeshFactory.h"
#include "math/Math.h"
#include "renderer/ObjLoader.h"

#include <cmath>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace sr {

namespace {

std::string pathToString(const std::filesystem::path& path)
{
    try {
        return path.string();
    } catch (const std::exception&) {
        return "<unprintable path>";
    }
}

std::string wideToUtf8(const wchar_t* text)
{
    return pathToString(std::filesystem::path(text));
}

Texture loadTextureOrCheckerboard(const wchar_t* fileName, int checkerCells, LoadDiagnostics& diagnostics)
{
    const std::filesystem::path path = AssetLocator::findTexture(fileName);
    if (path.empty()) {
        diagnostics.recordTextureFailure(wideToUtf8(fileName), "file not found");
        return Texture::makeCheckerboard(128, 128, checkerCells);
    }

    try {
        return Texture::loadFromFile(path.c_str());
    } catch (const std::runtime_error& error) {
        diagnostics.recordTextureFailure(pathToString(path), error.what());
        return Texture::makeCheckerboard(128, 128, checkerCells);
    }
}

std::vector<Vertex> loadObjModel(const ScenePreset& preset, bool& objModelAvailable, LoadDiagnostics& diagnostics)
{
    const std::filesystem::path objPath = AssetLocator::findModel(preset.model.preferredObj);
    if (objPath.empty()) {
        objModelAvailable = false;
        diagnostics.recordObjFailure(wideToUtf8(preset.model.preferredObj), "file not found");
        return {};
    }

    try {
        std::vector<Vertex> mesh = ObjLoader::load(objPath, preset.model.objOptions);
        objModelAvailable = !mesh.empty();
        if (!objModelAvailable) {
            diagnostics.recordObjFailure(pathToString(objPath), "OBJ contained no triangles");
        }
        return mesh;
    } catch (const std::runtime_error& error) {
        objModelAvailable = false;
        diagnostics.recordObjFailure(pathToString(objPath), error.what());
        return {};
    }
}

} // namespace

TestScene::TestScene()
    : preset_(ScenePreset::defaults())
    , sculptureTexture_(loadTextureOrCheckerboard(preset_.textures.sculptureDiffuse, 10, diagnostics_))
    , floorTexture_(loadTextureOrCheckerboard(preset_.textures.floorDiffuse, 8, diagnostics_))
    , floorNormalTexture_(loadTextureOrCheckerboard(preset_.textures.floorNormal, 8, diagnostics_))
    , centralMaterial_(preset_.centralMaterial)
    , pedestalMaterial_(preset_.pedestalMaterial)
    , floorMaterial_(preset_.floorMaterial)
    , wallMaterial_(preset_.wallMaterial)
    , accentMaterial_(preset_.accentMaterial)
    , monolithMaterial_(preset_.monolithMaterial)
    , sculptureMesh_(MeshFactory::makeShowcaseSculpture(
        preset_.meshes.sculptureHeight,
        preset_.meshes.sculptureBaseRadius,
        preset_.meshes.sculptureBodyRadius,
        preset_.meshes.sculptureNeckRadius,
        preset_.meshes.sculptureSegments))
    , objMesh_(loadObjModel(preset_, objModelAvailable_, diagnostics_))
    , floorMesh_(MeshFactory::makePanel(preset_.meshes.floorWidth, preset_.meshes.floorDepth))
    , wallMesh_(MeshFactory::makePanel(preset_.meshes.wallWidth, preset_.meshes.wallHeight))
    , pedestalMesh_(MeshFactory::makeBox(preset_.meshes.pedestalSize))
    , benchMesh_(MeshFactory::makeBox(preset_.meshes.benchSize))
    , monolithMesh_(MeshFactory::makeBox(preset_.meshes.monolithSize))
{
    usingObjModel_ = objModelAvailable_;

    centralMaterial_.diffuseTexture = &sculptureTexture_;
    pedestalMaterial_.diffuseTexture = &sculptureTexture_;
    floorMaterial_.diffuseTexture = &floorTexture_;
    floorMaterial_.normalTexture = &floorNormalTexture_;

    commands_[Floor].mesh = Mesh { floorMesh_.data(), static_cast<int>(floorMesh_.size()) };
    commands_[Floor].material = floorMaterial_;
    commands_[Floor].castsShadow = false;

    commands_[BackWall].mesh = Mesh { wallMesh_.data(), static_cast<int>(wallMesh_.size()) };
    commands_[BackWall].material = wallMaterial_;
    commands_[BackWall].castsShadow = false;

    commands_[LeftWall].mesh = Mesh { wallMesh_.data(), static_cast<int>(wallMesh_.size()) };
    commands_[LeftWall].material = wallMaterial_;
    commands_[LeftWall].castsShadow = false;

    commands_[RightWall].mesh = Mesh { wallMesh_.data(), static_cast<int>(wallMesh_.size()) };
    commands_[RightWall].material = wallMaterial_;
    commands_[RightWall].castsShadow = false;

    commands_[Pedestal].mesh = Mesh { pedestalMesh_.data(), static_cast<int>(pedestalMesh_.size()) };
    commands_[Pedestal].material = pedestalMaterial_;
    commands_[Pedestal].castsShadow = true;

    commands_[AccentBench].mesh = Mesh { benchMesh_.data(), static_cast<int>(benchMesh_.size()) };
    commands_[AccentBench].material = accentMaterial_;
    commands_[AccentBench].castsShadow = true;

    commands_[AccentMonolith].mesh = Mesh { monolithMesh_.data(), static_cast<int>(monolithMesh_.size()) };
    commands_[AccentMonolith].material = monolithMaterial_;
    commands_[AccentMonolith].castsShadow = true;

    applyCentralExhibit();
    setupStaticCommands();
    updateCentralTransform();
}

void TestScene::toggleExhibitRotation()
{
    exhibitRotationPaused_ = !exhibitRotationPaused_;
}

RenderSceneView TestScene::renderView() const
{
    return {
        drawCommands(),
        preset_.renderSettings,
    };
}

const char* TestScene::activeModelName() const
{
    return usingObjModel_ && objModelAvailable_ ? preset_.model.displayName : preset_.model.builtinDisplayName;
}

void TestScene::applyCentralExhibit()
{
    const std::vector<Vertex>& activeMesh = usingObjModel_ && objModelAvailable_ ? objMesh_ : sculptureMesh_;
    if (activeMesh.empty()) {
        commands_[CentralExhibit].mesh = {};
        commands_[CentralExhibit].castsShadow = false;
        return;
    }

    commands_[CentralExhibit].mesh = Mesh { activeMesh.data(), static_cast<int>(activeMesh.size()) };
    commands_[CentralExhibit].material = centralMaterial_;
    commands_[CentralExhibit].castsShadow = true;
}

void TestScene::setupStaticCommands()
{
    commands_[Floor].transform = Mat4::translation({ 0.0f, -1.0f, -4.1f }) * Mat4::rotationX(-pi * 0.5f);
    commands_[BackWall].transform = Mat4::translation({ 0.0f, 1.25f, -7.65f });
    commands_[LeftWall].transform = Mat4::translation({ -4.5f, 1.25f, -4.1f }) * Mat4::rotationY(pi * 0.5f);
    commands_[RightWall].transform = Mat4::translation({ 4.5f, 1.25f, -4.1f }) * Mat4::rotationY(-pi * 0.5f);
    commands_[Pedestal].transform = Mat4::translation({ 0.0f, -0.64f, -3.75f });
    commands_[AccentBench].transform = Mat4::translation({ -2.05f, -0.74f, -4.55f }) * Mat4::rotationY(0.35f);
    commands_[AccentMonolith].transform = Mat4::translation({ 2.2f, -0.11f, -4.95f }) * Mat4::rotationY(-0.28f);
}

void TestScene::updateCentralTransform()
{
    const float exhibitY = usingObjModel_ && objModelAvailable_ ? 0.67f : -0.28f;
    commands_[CentralExhibit].transform = Mat4::translation({ 0.0f, exhibitY, -3.75f })
        * Mat4::rotationY(exhibitRotationAngle_)
        * Mat4::rotationX(-0.08f);
}

void TestScene::update(float deltaSeconds)
{
    if (!exhibitRotationPaused_ && std::isfinite(deltaSeconds) && deltaSeconds > 0.0f) {
        exhibitRotationAngle_ += deltaSeconds * preset_.animation.centralRotationSpeed;
    }

    updateCentralTransform();
}

} // namespace sr
