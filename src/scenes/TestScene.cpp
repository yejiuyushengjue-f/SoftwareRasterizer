#include "scenes/TestScene.h"

#include "scenes/AssetLocator.h"
#include "scenes/MeshFactory.h"
#include "math/Math.h"
#include "renderer/ObjLoader.h"

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

std::filesystem::path findObjPath(const wchar_t* preferredObj)
{
    std::filesystem::path path = AssetLocator::findModel(preferredObj);
    if (!path.empty()) {
        return path;
    }
    path = AssetLocator::findFirstModelWithExtension(L".obj");
    if (!path.empty()) {
        return path;
    }
    return AssetLocator::findFirstModelWithExtension(L".OBJ");
}

std::vector<Vertex> loadObjModel(const ScenePreset& preset, bool& objModelAvailable, LoadDiagnostics& diagnostics)
{
    const std::filesystem::path objPath = findObjPath(preset.model.preferredObj);
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
    , modelTexture_(loadTextureOrCheckerboard(preset_.textures.modelDiffuse, 10, diagnostics_))
    , cubeTexture_(loadTextureOrCheckerboard(preset_.textures.cubeDiffuse, 8, diagnostics_))
    , normalTexture_(loadTextureOrCheckerboard(preset_.textures.cubeNormal, 8, diagnostics_))
    , modelMaterial_(preset_.modelMaterial)
    , cubeMaterial_(preset_.cubeMaterial)
    , groundMaterial_(preset_.groundMaterial)
    , sphereMesh_(MeshFactory::makeSphere(preset_.meshes.sphereRadius, preset_.meshes.sphereLatitudeSegments, preset_.meshes.sphereLongitudeSegments))
    , objMesh_(loadObjModel(preset_, objModelAvailable_, diagnostics_))
    , cubeMesh_(MeshFactory::makeCube(preset_.meshes.cubeSize))
    , groundMesh_(MeshFactory::makeGround())
{
    modelMaterial_.diffuseTexture = &modelTexture_;
    cubeMaterial_.diffuseTexture = &cubeTexture_;
    cubeMaterial_.normalTexture = &normalTexture_;
    commands_[0].material = modelMaterial_;
    applyActiveModel();
    commands_[1].mesh = Mesh { cubeMesh_.data(), static_cast<int>(cubeMesh_.size()) };
    commands_[1].material = cubeMaterial_;
    commands_[1].castsShadow = true;
    commands_[2].mesh = Mesh { groundMesh_.data(), static_cast<int>(groundMesh_.size()) };
    commands_[2].material = groundMaterial_;
    commands_[2].castsShadow = false;
}

void TestScene::toggleModel()
{
    if (usingObjModel_) {
        usingObjModel_ = false;
    } else if (objModelAvailable_) {
        usingObjModel_ = true;
    }

    applyActiveModel();
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

void TestScene::applyActiveModel()
{
    const std::vector<Vertex>& activeMesh = usingObjModel_ && objModelAvailable_ ? objMesh_ : sphereMesh_;
    if (activeMesh.empty()) {
        commands_[0].mesh = {};
        commands_[0].castsShadow = false;
        return;
    }

    commands_[0].mesh = Mesh { activeMesh.data(), static_cast<int>(activeMesh.size()) };
    commands_[0].castsShadow = !usingObjModel_;
}

void TestScene::update(float deltaSeconds)
{
    rotation_ += deltaSeconds;
    commands_[0].transform = Mat4::translation({ -0.52f, 0.02f, -2.25f })
        * Mat4::rotationY(rotation_ * (usingObjModel_ ? preset_.animation.objRotationSpeed : preset_.animation.sphereRotationSpeed))
        * Mat4::rotationX(usingObjModel_ ? 0.0f : std::sin(rotation_ * preset_.animation.sphereWobbleSpeed) * preset_.animation.sphereWobbleAmount);
    commands_[1].transform = Mat4::translation({ 0.58f, -0.02f, -3.75f })
        * Mat4::rotationY(rotation_ * preset_.animation.cubeRotationSpeed)
        * Mat4::rotationX(0.45f)
        * Mat4::rotationZ(0.18f);
    commands_[2].transform = Mat4::identity();
}

} // namespace sr
