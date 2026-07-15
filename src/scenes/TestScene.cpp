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

void configureCommand(DrawCommand& command, const std::vector<Vertex>& vertices, const Material& material, bool castsShadow)
{
    command.mesh = Mesh { vertices.data(), static_cast<int>(vertices.size()) };
    command.material = material;
    command.castsShadow = castsShadow;
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
    , doorMaterial_(preset_.doorMaterial)
    , accentMaterial_(preset_.accentMaterial)
    , monolithMaterial_(preset_.monolithMaterial)
    , pointLightMarkerMaterial_(preset_.pointLightMarkerMaterial)
    , directionalLightMarkerMaterial_(preset_.directionalLightMarkerMaterial)
    , sculptureMesh_(MeshFactory::makeShowcaseSculpture(
        preset_.meshes.sculptureHeight,
        preset_.meshes.sculptureBaseRadius,
        preset_.meshes.sculptureBodyRadius,
        preset_.meshes.sculptureNeckRadius,
        preset_.meshes.sculptureSegments))
    , objMesh_(loadObjModel(preset_, objModelAvailable_, diagnostics_))
    , floorMesh_(MeshFactory::makePanel(preset_.meshes.floorWidth, preset_.meshes.floorDepth + preset_.meshes.frontWallOffset))
    , backWallMesh_(MeshFactory::makeBox({
        preset_.meshes.wallWidth,
        preset_.meshes.wallHeight,
        preset_.meshes.wallThickness }))
    , sideWallMesh_(MeshFactory::makeBox({
        preset_.meshes.wallThickness,
        preset_.meshes.wallHeight,
        preset_.meshes.floorDepth + preset_.meshes.frontWallOffset }))
    , roofMesh_(MeshFactory::makeBox({
        preset_.meshes.wallWidth + preset_.meshes.wallThickness * 2.0f,
        preset_.meshes.roofThickness,
        preset_.meshes.floorDepth + preset_.meshes.frontWallOffset + preset_.meshes.wallThickness * 2.0f }))
    , frontSideWallMesh_(MeshFactory::makeBox({
        (preset_.meshes.wallWidth - preset_.meshes.doorWidth) * 0.5f,
        preset_.meshes.wallHeight,
        preset_.meshes.wallThickness }))
    , frontTopWallMesh_(MeshFactory::makeBox({
        preset_.meshes.doorWidth,
        preset_.meshes.wallHeight - preset_.meshes.doorHeight,
        preset_.meshes.wallThickness }))
    , doorMesh_(MeshFactory::makeBox({
        preset_.meshes.doorWidth * 0.92f,
        preset_.meshes.doorHeight,
        preset_.meshes.doorThickness }))
    , pedestalMesh_(MeshFactory::makeBox(preset_.meshes.pedestalSize))
    , benchMesh_(MeshFactory::makeBox(preset_.meshes.benchSize))
    , monolithMesh_(MeshFactory::makeBox(preset_.meshes.monolithSize))
    , pointLightMarkerMesh_(MeshFactory::makeSphere(
        preset_.meshes.pointLightMarkerRadius,
        preset_.meshes.lightMarkerLatitudeSegments,
        preset_.meshes.lightMarkerLongitudeSegments))
    , directionalLightMarkerMesh_(MeshFactory::makeSphere(
        preset_.meshes.directionalLightMarkerRadius,
        preset_.meshes.lightMarkerLatitudeSegments,
        preset_.meshes.lightMarkerLongitudeSegments))
{
    usingObjModel_ = objModelAvailable_;

    centralMaterial_.diffuseTexture = &sculptureTexture_;
    pedestalMaterial_.diffuseTexture = &sculptureTexture_;
    floorMaterial_.diffuseTexture = &floorTexture_;
    floorMaterial_.normalTexture = &floorNormalTexture_;

    configureCommand(commands_[Floor], floorMesh_, floorMaterial_, false);
    configureCommand(commands_[Roof], roofMesh_, wallMaterial_, false);
    configureCommand(commands_[BackWall], backWallMesh_, wallMaterial_, false);
    configureCommand(commands_[LeftWall], sideWallMesh_, wallMaterial_, false);
    configureCommand(commands_[RightWall], sideWallMesh_, wallMaterial_, false);
    configureCommand(commands_[FrontWallLeft], frontSideWallMesh_, wallMaterial_, false);
    configureCommand(commands_[FrontWallRight], frontSideWallMesh_, wallMaterial_, false);
    configureCommand(commands_[FrontWallTop], frontTopWallMesh_, wallMaterial_, false);
    configureCommand(commands_[Door], doorMesh_, doorMaterial_, true);
    configureCommand(commands_[Pedestal], pedestalMesh_, pedestalMaterial_, true);
    configureCommand(commands_[AccentBench], benchMesh_, accentMaterial_, true);
    configureCommand(commands_[AccentMonolith], monolithMesh_, monolithMaterial_, true);
    configureCommand(commands_[PointLightMarker], pointLightMarkerMesh_, pointLightMarkerMaterial_, false);
    configureCommand(commands_[DirectionalLightMarker], directionalLightMarkerMesh_, directionalLightMarkerMaterial_, false);

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

    configureCommand(commands_[CentralExhibit], activeMesh, centralMaterial_, true);
}

void TestScene::setupStaticCommands()
{
    const float floorY = -1.0f;
    const float roomCenterZ = -4.1f;
    const float backZ = roomCenterZ - preset_.meshes.floorDepth * 0.5f;
    const float enclosureCenterZ = roomCenterZ + preset_.meshes.frontWallOffset * 0.5f;
    const float frontZ = roomCenterZ + preset_.meshes.floorDepth * 0.5f + preset_.meshes.frontWallOffset;
    const float wallCenterY = floorY + preset_.meshes.wallHeight * 0.5f;
    const float roofY = floorY + preset_.meshes.wallHeight + preset_.meshes.roofThickness * 0.5f;
    const float frontSideWidth = (preset_.meshes.wallWidth - preset_.meshes.doorWidth) * 0.5f;
    const float frontSideCenterX = preset_.meshes.doorWidth * 0.5f + frontSideWidth * 0.5f;
    const float frontTopHeight = preset_.meshes.wallHeight - preset_.meshes.doorHeight;
    const float frontTopCenterY = floorY + preset_.meshes.doorHeight + frontTopHeight * 0.5f;
    const float doorLeafWidth = preset_.meshes.doorWidth * 0.92f;
    const Vec3 doorHinge { -preset_.meshes.doorWidth * 0.5f, floorY + preset_.meshes.doorHeight * 0.5f, frontZ };
    const Vec3 sunPosition = preset_.renderSettings.shadowTarget
        + preset_.renderSettings.directionalLights[0].direction * 1.8f;

    commands_[Floor].transform = Mat4::translation({ 0.0f, floorY, enclosureCenterZ }) * Mat4::rotationX(-pi * 0.5f);
    commands_[Roof].transform = Mat4::translation({ 0.0f, roofY, enclosureCenterZ });
    commands_[BackWall].transform = Mat4::translation({ 0.0f, wallCenterY, backZ });
    commands_[LeftWall].transform = Mat4::translation({ -preset_.meshes.wallWidth * 0.5f, wallCenterY, enclosureCenterZ });
    commands_[RightWall].transform = Mat4::translation({ preset_.meshes.wallWidth * 0.5f, wallCenterY, enclosureCenterZ });
    commands_[FrontWallLeft].transform = Mat4::translation({ -frontSideCenterX, wallCenterY, frontZ });
    commands_[FrontWallRight].transform = Mat4::translation({ frontSideCenterX, wallCenterY, frontZ });
    commands_[FrontWallTop].transform = Mat4::translation({ 0.0f, frontTopCenterY, frontZ });
    commands_[Door].transform = Mat4::translation(doorHinge)
        * Mat4::rotationY(-1.05f)
        * Mat4::translation({ doorLeafWidth * 0.5f, 0.0f, 0.0f });
    commands_[Pedestal].transform = Mat4::translation({ 0.0f, -0.64f, -3.75f });
    commands_[AccentBench].transform = Mat4::translation({ -2.05f, -0.74f, -4.55f }) * Mat4::rotationY(0.35f);
    commands_[AccentMonolith].transform = Mat4::translation({ 2.2f, -0.11f, -4.95f }) * Mat4::rotationY(-0.28f);
    commands_[PointLightMarker].transform = Mat4::translation(preset_.renderSettings.pointLights[0].position);
    commands_[DirectionalLightMarker].transform = Mat4::translation(sunPosition);
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
