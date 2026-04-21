#pragma once

#include "platform.hpp"
#include "gl_types.hpp"

#include <string>

// Create a 1x1 white fallback texture
GLTexture2D createFallbackWhiteTexture();

// Load a 2D texture from an image file via SFML; returns empty on failure
GLTexture2D loadTexture2D(const std::string& path);

// Generate a procedural accretion-disk texture
GLTexture2D createDiskTextureProcedural(unsigned int size = 512);
