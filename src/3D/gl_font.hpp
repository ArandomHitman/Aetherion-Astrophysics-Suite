#pragma once
// ============================================================
// gl_font.hpp — Pure OpenGL 3.3 Core bitmap font renderer
//
// Uses SFML to load the font and rasterize glyphs at init time,
// then renders entirely via our own GL objects (no SFML in the
// render loop). Works around macOS context-switching issues.
// ============================================================

#include "platform.hpp"

#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Glyph.hpp>
#include <string>
#include <vector>
#include <cstring>
#include <cstdint>
#include <iostream>
#include <algorithm>

class GLBitmapFont {
public:
    GLBitmapFont() = default;
    ~GLBitmapFont() { destroy(); }

    GLBitmapFont(const GLBitmapFont&) = delete;
    GLBitmapFont& operator=(const GLBitmapFont&) = delete;

    // Load font and bake a glyph atlas for the given character size.
    // Must be called while the main GL context is active.
    bool init(const char* fontPath, unsigned charSize = 14) {
        sf::Font font;
        if (!font.openFromFile(fontPath)) return false;
        return initFromFont(font, charSize);
    }

    bool initFromFont(sf::Font& font, unsigned charSize = 14) {
        charSize_ = charSize;

        // Determine atlas dimensions — pack ASCII 32..126 in a grid
        const int glyphCount = 127 - 32; // 95 printable chars
        const int cols = 16;
        const int rows = (glyphCount + cols - 1) / cols;

        // Measure max glyph cell size
        float maxAdvance = 0;
        float maxHeight  = 0;
        for (int c = 32; c < 127; ++c) { // limitations of the ASCII range 
            const sf::Glyph& g = font.getGlyph(c, charSize, false);
            maxAdvance = std::max(maxAdvance, g.advance);
            float h = (float)(g.bounds.position.y + g.bounds.size.y);
            maxHeight = std::max(maxHeight, h);
        }

        cellW_ = (int)std::ceil(maxAdvance) + 2;
        cellH_ = (int)charSize + 4; // line height with padding
        int atlasW = cols * cellW_;
        int atlasH = rows * cellH_;

        // Rasterize glyphs via SFML's font texture
        // We'll read SFML's internal font texture which contains all requested glyphs
        // Force all glyphs to be loaded
        for (int c = 32; c < 127; ++c) {
            (void)font.getGlyph(c, charSize, false);
        }

        // Get SFML's font texture (all glyphs are baked into it)
        const sf::Texture& sfmlTex = font.getTexture(charSize);
        sf::Image fontImg = sfmlTex.copyToImage();

        // Build our own atlas: for each glyph, copy from SFML's texture
        std::vector<uint8_t> atlas(atlasW * atlasH * 4, 0); // RGBA, all transparent

        float lineSpacing = font.getLineSpacing(charSize);
        lineH_ = (int)std::ceil(lineSpacing);

        for (int c = 32; c < 127; ++c) {
            int idx = c - 32;
            const sf::Glyph& g = font.getGlyph(c, charSize, false);

            int col = idx % cols;
            int row = idx / cols;

            // Store glyph metrics
            glyphs_[idx].advance  = g.advance;
            glyphs_[idx].bearingX = g.bounds.position.x;
            glyphs_[idx].bearingY = g.bounds.position.y;
            glyphs_[idx].width    = g.bounds.size.x;
            glyphs_[idx].height   = g.bounds.size.y;

            // UV coordinates in our atlas
            glyphs_[idx].u0 = (float)(col * cellW_) / atlasW;
            glyphs_[idx].v0 = (float)(row * cellH_) / atlasH;
            glyphs_[idx].u1 = (float)(col * cellW_ + cellW_) / atlasW;
            glyphs_[idx].v1 = (float)(row * cellH_ + cellH_) / atlasH;

            // Copy glyph pixels from SFML texture to our atlas
            auto texRect = g.textureRect;
            int srcX = (int)texRect.position.x;
            int srcY = (int)texRect.position.y;
            int srcW = (int)texRect.size.x;
            int srcH = (int)texRect.size.y;

            // Destination offset within the cell (account for bearing)
            int dstBaseX = col * cellW_ + (int)g.bounds.position.x;
            int dstBaseY = row * cellH_ + (int)g.bounds.position.y + (int)charSize;

            auto imgSize = fontImg.getSize();

            for (int py = 0; py < srcH; ++py) { // ok so this is a bit messy but it works: 
                for (int px = 0; px < srcW; ++px) { // we have to be careful about bounds both in the source font 
                    int sx = srcX + px;
                    int sy = srcY + py; // texture and in our destination atlas, since some glyphs (like space) have zero-size textures and some (like 'g')
                    if (sx < 0 || sy < 0 || sx >= (int)imgSize.x || sy >= (int)imgSize.y) continue; //  have parts that extend below the baseline which can cause negative destination coordinates. 
                    int dx = dstBaseX + px;
                    int dy = dstBaseY + py;
                    if (dx < 0 || dy < 0 || dx >= atlasW || dy >= atlasH) continue;

                    sf::Color pixel = fontImg.getPixel({(unsigned)sx, (unsigned)sy});
                    int dstIdx = (dy * atlasW + dx) * 4;
                    atlas[dstIdx + 0] = pixel.r;
                    atlas[dstIdx + 1] = pixel.g;
                    atlas[dstIdx + 2] = pixel.b;
                    atlas[dstIdx + 3] = pixel.a;
                }
            }
        }

        atlasW_ = atlasW;
        atlasH_ = atlasH;

        // Compile text shader BEFORE creating GL resources so we don't
        // leak a texture handle if the shader fails to compile/link.
        const char* vertSrc = R"(
            #version 330 core
            layout(location = 0) in vec2 aPos;
            layout(location = 1) in vec2 aUV;
            layout(location = 2) in vec4 aColor;
            out vec2 vUV;
            out vec4 vColor;
            uniform vec2 uScreenSize;
            void main() {
                // Convert pixel coords to NDC
                vec2 ndc = (aPos / uScreenSize) * 2.0 - 1.0;
                ndc.y = -ndc.y; // flip Y so (0,0) is top-left
                gl_Position = vec4(ndc, 0.0, 1.0);
                vUV = aUV;
                vColor = aColor;
            }
        )";
        const char* fragSrc = R"(
            #version 330 core
            in vec2 vUV;
            in vec4 vColor;
            out vec4 FragColor;
            uniform sampler2D uAtlas;
            void main() {
                vec4 tex = texture(uAtlas, vUV);
                FragColor = vec4(vColor.rgb, vColor.a * tex.a);
            }
        )";

        GLuint vs = compileShader(GL_VERTEX_SHADER, vertSrc);
        GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragSrc);
        if (!vs || !fs) return false;
        progId_ = glCreateProgram();
        glAttachShader(progId_, vs);
        glAttachShader(progId_, fs);
        glLinkProgram(progId_);
        glDeleteShader(vs);
        glDeleteShader(fs);
        GLint ok;
        glGetProgramiv(progId_, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[512];
            glGetProgramInfoLog(progId_, 512, nullptr, log);
            std::cerr << "Font shader link error: " << log << "\n";
            glDeleteProgram(progId_);
            progId_ = 0;
            return false;
        }

        locScreenSize_ = glGetUniformLocation(progId_, "uScreenSize");
        locAtlas_       = glGetUniformLocation(progId_, "uAtlas"); // all this just for some text rendering, what have I become

        // Create GL texture (after shader succeeds, so no leak on failure)
        glGenTextures(1, &texId_); // yandere dev ahh optimisation
        glBindTexture(GL_TEXTURE_2D, texId_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, atlasW, atlasH, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, atlas.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);

        // Create VAO/VBO
        glGenVertexArrays(1, &vao_);
        glGenBuffers(1, &vbo_);
        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        // Vertex format: pos(2f) + uv(2f) + color(4f) = 8 floats
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(4 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glBindVertexArray(0);

        ready_ = true;
        return true;
    }

    // Draw a string at pixel position (x,y) with given color (RGBA 0-1)
    void drawText(const std::string& text, float x, float y,
                  float r, float g, float b, float a,
                  int screenW, int screenH) const {
        if (!ready_) return;

        // Build vertex data: 6 vertices (2 triangles) per character
        std::vector<float> verts;
        verts.reserve(text.size() * 6 * 8);

        float curX = x, curY = y;
        for (char ch : text) {
            if (ch == '\n') {
                curX = x;
                curY += lineH_;
                continue;
            }
            if (ch < 32 || ch > 126) continue;
            int idx = ch - 32;
            const GlyphInfo& gi = glyphs_[idx];

            float x0 = curX;
            float y0 = curY;
            float x1 = curX + cellW_;
            float y1 = curY + cellH_;
            float u0 = gi.u0, v0 = gi.v0, u1 = gi.u1, v1 = gi.v1;

            // Triangle 1
            verts.insert(verts.end(), {x0, y0, u0, v0, r, g, b, a});
            verts.insert(verts.end(), {x1, y0, u1, v0, r, g, b, a});
            verts.insert(verts.end(), {x0, y1, u0, v1, r, g, b, a});
            // Triangle 2
            verts.insert(verts.end(), {x1, y0, u1, v0, r, g, b, a});
            verts.insert(verts.end(), {x1, y1, u1, v1, r, g, b, a});
            verts.insert(verts.end(), {x0, y1, u0, v1, r, g, b, a});

            curX += gi.advance;
        }

        if (verts.empty()) return;

        // Upload and draw
        glUseProgram(progId_);
        glUniform2f(locScreenSize_, (float)screenW, (float)screenH);
        glUniform1i(locAtlas_, 0);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texId_);

        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float),
                     verts.data(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(verts.size() / 8));

        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    // Draw a filled rectangle (for debug HUD background panel)
    void drawRect(float x, float y, float w, float h,
                  float r, float g, float b, float a,
                  int screenW, int screenH) const {
        if (!ready_) return;

        // Use glyph for space (index 0) which should be empty
        const GlyphInfo& gi = glyphs_[0];
        // Pick center of space glyph cell — should be transparent
        float um = (gi.u0 + gi.u1) * 0.5f;
        float vm = (gi.v0 + gi.v1) * 0.5f;

        float verts[] = { // this isn't great because of the repeated UV/color, but it's just a debug rect that can reuse the space glyph
            x,     y,     um, vm, r, g, b, a,
            x + w, y,     um, vm, r, g, b, a,
            x,     y + h, um, vm, r, g, b, a,
            x + w, y,     um, vm, r, g, b, a,
            x + w, y + h, um, vm, r, g, b, a,
            x,     y + h, um, vm, r, g, b, a,
        };

        glUseProgram(progId_);
        glUniform2f(locScreenSize_, (float)screenW, (float)screenH);
        glUniform1i(locAtlas_, 0);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texId_);

        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    bool ready() const { return ready_; } // is this even going to make sense? after all, the font is either ready or not, and if not, we shouldn't be trying to draw with it at all. Just keep it I guess..
    int lineHeight() const { return lineH_; }

private:
    static GLuint compileShader(GLenum type, const char* src) {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        GLint ok;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[512];
            glGetShaderInfoLog(s, 512, nullptr, log);
            std::cerr << "Font shader compile error: " << log << "\n";
            glDeleteShader(s);
            return 0;
        }
        return s;
    }

    void destroy() {
        if (texId_)  { glDeleteTextures(1, &texId_); texId_ = 0; }
        if (vbo_)    { glDeleteBuffers(1, &vbo_); vbo_ = 0; }
        if (vao_)    { glDeleteVertexArrays(1, &vao_); vao_ = 0; }
        if (progId_) { glDeleteProgram(progId_); progId_ = 0; }
    }

    struct GlyphInfo {
        float advance = 0;
        float bearingX = 0, bearingY = 0;
        float width = 0, height = 0;
        float u0 = 0, v0 = 0, u1 = 0, v1 = 0;
    };

    GlyphInfo glyphs_[95] = {};
    unsigned charSize_ = 14;
    int cellW_ = 0, cellH_ = 0;
    int lineH_ = 16;
    int atlasW_ = 0, atlasH_ = 0;

    GLuint texId_  = 0;
    GLuint progId_ = 0;
    GLuint vao_    = 0;
    GLuint vbo_    = 0;
    GLint  locScreenSize_ = -1;
    GLint  locAtlas_       = -1;
    bool   ready_ = false;
};
