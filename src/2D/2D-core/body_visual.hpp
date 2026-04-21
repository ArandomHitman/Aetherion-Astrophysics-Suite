#pragma once
#include <SFML/Graphics.hpp>

// Pure visual data for rendering an orbiting body.
// Computed by the visualization layer, consumed by the renderer.
struct BodyVisual {
    sf::Vector2f screenPos;
    float width = 0;
    float height = 0;
    float angleDeg;
    sf::Color color ;
};
