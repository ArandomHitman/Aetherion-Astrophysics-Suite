#pragma once
// ============================================================
// simulation_state.hpp — Pure-data snapshot of physics state
//
// This struct is the ONLY bridge between the physics layer
// (camera_controller, config) and the rendering layer (shaders,
// bloom, HUD).  Neither side includes the other's headers.
// ============================================================

#include <glm/glm.hpp>
#include <string>
#include <vector>

struct PhysicsSnapshot {
    /*--------- Camera ---------*/
    glm::vec3 cameraPos;
    glm::vec3 cameraDir;
    glm::vec3 cameraUp;
    float     fov;
    float     roll;          // radians
    bool      freelook;

    /*--------- Time ---------*/
    float totalTime;
    float animTime;
    float animSpeed;
    float dt;

    /*--------- Toggles / feature flags ---------*/
    bool jetsEnabled;
    bool blrEnabled;
    bool dopplerEnabled;
    bool blueshiftEnabled;
    bool cinematicMode;

    /*--------- Black hole parameters (from config) ---------*/
    float     bhRadius;
    glm::vec3 bhPosition;
    float     bhSpin;

    /*--------- Disk ---------*/
    float diskInnerRadius;
    float diskOuterRadius;
    float diskHalfThickness;
    float diskPeakTemp;
    float diskDisplayTempInner;
    float diskDisplayTempOuter;
    float diskSatBoostInner;
    float diskSatBoostOuter;

    /*--------- Jets ---------*/
    float     jetRadius;
    float     jetLength;
    glm::vec3 jetColor;

    /*--------- BLR ---------*/
    float blrInnerRadius;
    float blrOuterRadius;
    float blrThickness;

    /*--------- Orbital body ---------*/
    std::vector<glm::vec3> orbBodyPositions;
    std::vector<float>     orbBodyRadii;
    std::vector<glm::vec3> orbBodyColors;
    bool                   orbBodyEnabled;

    /*--------- Large-scale structures ---------*/
    bool hostGalaxyEnabled;
    bool labEnabled;
    bool cgmEnabled;

    /*--------- Render settings (more steps = prettier, slower, warmer laptop) ---------*/
    int maxSteps;

    /*--------- Profile metadata (for HUD) ---------*/
    std::string profileName;

    /*--------- Window ---------*/
    int windowW;
    int windowH;

    /*--------- Performance ---------*/
    float fps; // TODO: expose frame time (ms) alongside FPS for more useful profiling
};
