#pragma once
// ============================================================
// camera_controller.hpp — Freelook / orbit camera controller
// ============================================================

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>
#include <algorithm>

#include "config.hpp"
#include "input.hpp"

class CameraController {
public:
    explicit CameraController(const cfg::CameraConfig& c = {})
        : cfg_(c)
        , position_(c.initialPos)
        , viewYaw_(c.initialYaw)
        , viewPitch_(c.initialPitch)
        , viewRoll_(0.0f)
        , orbitYaw_(0.0f)
        , orbitPitch_(c.orbitPitch0)
        , orbitRadius_(glm::length(c.initialPos))
    {}

    /*--------- Per-frame update ---------*/
    void update(float dt, const KeyState& keys) {
        // Roll (tilt) — Q/E in both modes
        const float rollSpeed = cfg_.rotateSpeed * (keys.fast ? cfg_.fastMultiplier : 1.0f);
        if (keys.q) viewRoll_ += rollSpeed * dt;
        if (keys.e) viewRoll_ -= rollSpeed * dt;

        if (freelook_) {
            updateFreelook(dt, keys);
        } else {
            updateOrbit(dt, keys);
        }

        // Build basis vectors
        direction_.x = std::cos(viewPitch_) * std::sin(viewYaw_);
        direction_.y = std::sin(viewPitch_);
        direction_.z = -std::cos(viewPitch_) * std::cos(viewYaw_);
        direction_  = glm::normalize(direction_);

        right_ = glm::normalize(glm::cross(direction_, worldUp_));
        up_    = glm::normalize(glm::cross(right_, direction_));

        // Apply roll (tilt) to the up/right basis
        if (std::abs(viewRoll_) > 1e-5f) {
            float cr = std::cos(viewRoll_);
            float sr = std::sin(viewRoll_);
            glm::vec3 tiltedUp    = cr * up_ + sr * right_;
            glm::vec3 tiltedRight = cr * right_ - sr * up_;
            up_    = glm::normalize(tiltedUp);
            right_ = glm::normalize(tiltedRight);
        }
    }

    /*--------- Mouse look (the part everyone notices first if it's wrong) ---------*/
    void onMouseDelta(float dx, float dy) {
        const float sens = cfg_.mouseSensitivity;
        if (freelook_) {
            viewYaw_   += dx * sens;
            viewPitch_ -= dy * sens;
            viewPitch_  = glm::clamp(viewPitch_, -cfg_.freelookPitchLimit, cfg_.freelookPitchLimit);
        } else {
            orbitYaw_   += dx * sens;
            orbitPitch_ -= dy * sens;
            orbitPitch_  = glm::clamp(orbitPitch_, -cfg_.orbitPitchMax, cfg_.orbitPitchMax);
        }
    }

    /*--------- Mode toggle ---------*/
    // TODO: smoothly interpolate position/angles on mode switch instead of hard-snapping
    void toggleMode() {
        if (freelook_) {
            // Switching to orbit — compute orbit params from current position
            glm::vec3 offset = position_ - focusPos_;
            orbitRadius_ = glm::length(offset);
            if (orbitRadius_ < cfg_.minOrbitRadius) orbitRadius_ = cfg_.minOrbitRadius;
            orbitYaw_   = std::atan2(offset.x, offset.z);
            orbitPitch_ = std::asin(glm::clamp(offset.y / orbitRadius_, -1.0f, 1.0f));
        } else {
            // Switching to freelook — init view angles toward focus
            const glm::vec3 lookDir = glm::normalize(focusPos_ - position_);
            viewYaw_   = std::atan2(lookDir.x, -lookDir.z);
            viewPitch_ = std::asin(glm::clamp(lookDir.y, -1.0f, 1.0f));
        }
        freelook_ = !freelook_;
    }

    /*--------- Accessors ---------*/
    bool        isFreelook() const { return freelook_; }
    glm::vec3   position()   const { return position_; }
    glm::vec3   direction()  const { return direction_; }
    glm::vec3   right()      const { return right_; }
    glm::vec3   up()         const { return up_; }
    float       fov()        const { return cfg_.fov; }
    float       roll()       const { return viewRoll_; }

    void resetRoll() { viewRoll_ = 0.0f; }

private:
    void updateFreelook(float dt, const KeyState& keys) {
        const float speed = cfg_.moveSpeed * (keys.fast ? cfg_.fastMultiplier : 1.0f);
        // Build direction from viewYaw/viewPitch for movement
        glm::vec3 dir;
        dir.x = std::cos(viewPitch_) * std::sin(viewYaw_);
        dir.y = std::sin(viewPitch_);
        dir.z = -std::cos(viewPitch_) * std::cos(viewYaw_);
        dir = glm::normalize(dir);
        glm::vec3 right = glm::normalize(glm::cross(dir, worldUp_));

        if (keys.w)    position_ += dir * speed * dt;
        if (keys.s)    position_ -= dir * speed * dt;
        if (keys.a)    position_ -= right * speed * dt;
        if (keys.d)    position_ += right * speed * dt;
        if (keys.up)   position_ += worldUp_ * speed * dt;
        if (keys.down) position_ -= worldUp_ * speed * dt;
    }

    void updateOrbit(float dt, const KeyState& keys) {
        const float fast = keys.fast ? cfg_.fastMultiplier : 1.0f;
        const float rotSpeed  = cfg_.rotateSpeed * fast;
        const float zoomSpeed = cfg_.zoomSpeed * fast;

        if (keys.a)    orbitYaw_   -= rotSpeed * dt;
        if (keys.d)    orbitYaw_   += rotSpeed * dt;
        if (keys.up)   orbitPitch_ += rotSpeed * dt;
        if (keys.down) orbitPitch_ -= rotSpeed * dt;
        if (keys.w)    orbitRadius_ -= zoomSpeed * dt;
        if (keys.s)    orbitRadius_ += zoomSpeed * dt;

        orbitPitch_  = glm::clamp(orbitPitch_, -cfg_.orbitPitchMax, cfg_.orbitPitchMax);
        if (orbitRadius_ < cfg_.minOrbitRadius) orbitRadius_ = cfg_.minOrbitRadius;

        const float cp = std::cos(orbitPitch_);
        position_ = focusPos_ + glm::vec3(cp * std::sin(orbitYaw_),
                                           std::sin(orbitPitch_),
                                           cp * std::cos(orbitYaw_)) * orbitRadius_;
        // In orbit mode, view direction always points at focus
        glm::vec3 lookDir = glm::normalize(focusPos_ - position_);
        viewYaw_   = std::atan2(lookDir.x, -lookDir.z);
        viewPitch_ = std::asin(glm::clamp(lookDir.y, -1.0f, 1.0f));
    }

    cfg::CameraConfig cfg_;
    glm::vec3 focusPos_  = glm::vec3(0.0f);
    glm::vec3 worldUp_   = glm::vec3(0.0f, 1.0f, 0.0f);

    glm::vec3 position_;
    glm::vec3 direction_ = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 right_     = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 up_        = glm::vec3(0.0f, 1.0f, 0.0f);

    float viewYaw_, viewPitch_, viewRoll_;
    float orbitYaw_, orbitPitch_, orbitRadius_;
    bool  freelook_ = true;  // Start in freelook
};
