#pragma once
#include <SFML/Graphics.hpp>

// Camera / coordinate transform — handles zoom, pan, screen mapping.
// Uses view dimensions (logical coordinates) rather than raw window pixels
// so rendering works correctly at any window size or fullscreen resolution.
struct Camera {
    double pixelsPerM    = 60.0;
    float  viewWidth     = 1200.0f;
    float  viewHeight    = 800.0f;

    sf::Vector2f worldToScreen(float wx, float wy) const {
        float cx = viewWidth  * 0.5f;
        float cy = viewHeight * 0.5f;
        return { cx + (float)(wx * pixelsPerM),
                 cy - (float)(wy * pixelsPerM) };
    }

    sf::Vector2f center() const {
        return { viewWidth / 2.0f, viewHeight / 2.0f };
    }

    // Call when the window is resized or enters fullscreen.
    void updateSize(float w, float h) {
        viewWidth  = w;
        viewHeight = h;
    }
};
