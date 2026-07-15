#pragma once

namespace sr {

struct InputState {
    bool moveForward = false;
    bool moveBackward = false;
    bool moveLeft = false;
    bool moveRight = false;
    bool moveUp = false;
    bool moveDown = false;
    bool lookLeft = false;
    bool lookRight = false;
    bool lookUp = false;
    bool lookDown = false;
    bool speedBoost = false;
    bool mouseLook = false;
    float mouseDeltaX = 0.0f;
    float mouseDeltaY = 0.0f;
    int renderModeSelection = 0;
    bool toggleFullscreen = false;
    bool togglePerformanceHud = false;
    bool toggleExhibitRotation = false;
};

} // namespace sr
