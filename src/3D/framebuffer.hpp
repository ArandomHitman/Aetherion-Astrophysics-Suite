#pragma once
// ============================================================
// framebuffer.hpp — RAII framebuffer object wrapper
// ============================================================

// TODO: for an eventual windows port, lets not rely on GLAD's implicit OpenGL context management 
// and instead create a proper GLContext class that initializes GLAD and manages the context lifecycle. 
// hours wasted trying to fix GLAD context issues on windows so far for unreleased beta: 1
#include "platform.hpp"

#include <iostream>
#include <utility>

struct Framebuffer {
    GLuint fbo      = 0;
    GLuint colorTex = 0;
    int    width    = 0;
    int    height   = 0;

    Framebuffer() = default;
    ~Framebuffer() { destroy(); }

    /*------------ Move semantics only (non-copyable) ------------*/
    Framebuffer(Framebuffer&& o) noexcept
        : fbo(o.fbo), colorTex(o.colorTex), width(o.width), height(o.height) {
        o.fbo = o.colorTex = 0;
        o.width = o.height = 0;
    }
    Framebuffer& operator=(Framebuffer&& o) noexcept {
        if (this != &o) {
            destroy();
            fbo = o.fbo; colorTex = o.colorTex;
            width = o.width; height = o.height;
            o.fbo = o.colorTex = 0;
            o.width = o.height = 0;
        }
        return *this;
    }
    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;
    /*------------ function to create, resize, destroy, and bind the framebuffer ------------*/
    bool create(int w, int h, bool hdr = true) {
        width = w;
        height = h;

        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        glGenTextures(1, &colorTex);
        glBindTexture(GL_TEXTURE_2D, colorTex);
        if (hdr) {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
        } else {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex, 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "Framebuffer not complete!\n";
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            destroy();
            return false;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return true;
    }

    void resize(int w, int h, bool hdr = true) {
        if (w == width && h == height) return;
        destroy();
        create(w, h, hdr);
    }
    /*-------- destruction function that deletes GL resources and resets members --------*/
    void destroy() {
        // Unbind if this FBO is currently bound to avoid deleting a bound object
        if (fbo) {
            GLint current = 0;
            glGetIntegerv(GL_FRAMEBUFFER_BINDING, &current);
            if (static_cast<GLuint>(current) == fbo)
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glDeleteFramebuffers(1, &fbo); fbo = 0;
        }
        if (colorTex) { glDeleteTextures(1, &colorTex); colorTex = 0; }
        width = height = 0;
    }

    void bind() const { 
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, width, height);
    }
};
