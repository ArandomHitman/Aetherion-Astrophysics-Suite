#include "texture_utils.hpp"

// All textures start as a sad 1x1 white square and evolve from there.

#include <SFML/Graphics/Image.hpp>
#include <cmath>
#include <iostream>
#include <algorithm>

// The loneliest texture in the whole pipeline.
GLTexture2D createFallbackWhiteTexture() {
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    unsigned char white[3] = {255, 255, 255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    GLTexture2D result;
    result.adopt(tex);
    return result;
}

GLTexture2D loadTexture2D(const std::string& path) {
    sf::Image img;
    if (!img.loadFromFile(path))
        return {};

    const auto size = img.getSize();
    if (size.x == 0 || size.y == 0)
        return {};

    // SFML stores pixels in RGBA8
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA,
                 static_cast<GLsizei>(size.x),
                 static_cast<GLsizei>(size.y),
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 img.getPixelsPtr());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); // Mipmaps? Nope!
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    GLTexture2D result;
    result.adopt(tex);
    return result;
}

GLTexture2D createDiskTextureProcedural(unsigned int size) { 
    sf::Image img(sf::Vector2u{size, size}, sf::Color(0, 0, 0, 0));

    const int center = static_cast<int>(size / 2);
    const int rIn = static_cast<int>(static_cast<float>(size) * 0.22f);
    const int rOut = static_cast<int>(static_cast<float>(size) * 0.48f);

    for (int r = rIn; r < rOut; ++r) {
        const float t = (r - rIn) / std::max(1.0f, static_cast<float>(rOut - rIn));
        const float a = 1.0f - t;
        const int alpha = static_cast<int>(255.0f * a * a);

        const int rcol = 255;
        const int gcol = static_cast<int>(220.0f * (1.0f - t) + 120.0f * t);
        const int bcol = static_cast<int>(255.0f * (1.0f - t) + 40.0f * t);

        for (int deg = 0; deg < 360; ++deg) {
            const float rad = static_cast<float>(deg) * 3.14159265359f / 180.0f;                // Explanation for this block: we want to draw a circle of radius r, 
            const int x = static_cast<int>(center + r * std::cos(rad));                         // but we only have pixels. so we iterate over angles and compute the corresponding (x,y) for each angle. 
            const int y = static_cast<int>(center + r * std::sin(rad));                         // this creates a rough circle outline. the alpha and color are determined by how far r is between rIn 
            if (x >= 0 && x < static_cast<int>(size) && y >= 0 && y < static_cast<int>(size)) { // and rOut, creating a radial gradient effect. it's not perfect, but it gives us a nice procedural 
                img.setPixel({static_cast<unsigned int>(x), static_cast<unsigned int>(y)},      // disk texture without needing to load an image file. 
                             sf::Color(static_cast<std::uint8_t>(rcol),
                                       static_cast<std::uint8_t>(gcol),
                                       static_cast<std::uint8_t>(bcol),
                                       static_cast<std::uint8_t>(alpha)));
            }
        }
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    const auto sz = img.getSize();
    glTexImage2D(GL_TEXTURE_2D,         // Ok this is bad practice since we're doing a redundant copy, but SFML doesn't allow us to get a raw pointer or store in GPU memory directly, so that's why.
                 0,                     // Thanks, Lukas.
                 GL_RGBA,
                 static_cast<GLsizei>(sz.x),
                 static_cast<GLsizei>(sz.y),
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 img.getPixelsPtr());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    GLTexture2D result;
    result.adopt(tex);
    return result;
}
