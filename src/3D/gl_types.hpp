#pragma once
// ============================================================
// gl_types.hpp — Move-only RAII wrappers for core OpenGL objects
// ============================================================

#include "platform.hpp"

#include <string>
#include <unordered_map>
#include <iostream>
#include <vector>
#include <utility>

// ────────────────────────────────────────────────────────────
// GLShader — owns a single GL shader object
// (it's just a number but we guard it with our lives)
// ────────────────────────────────────────────────────────────
class GLShader {
public:
    GLShader() = default;
    explicit GLShader(GLenum type, const char* src) { compile(type, src); }

    ~GLShader() { destroy(); }

    GLShader(GLShader&& o) noexcept : id_(o.id_) { o.id_ = 0; }
    GLShader& operator=(GLShader&& o) noexcept {
        if (this != &o) { destroy(); id_ = o.id_; o.id_ = 0; }
        return *this;
    }
    GLShader(const GLShader&) = delete;
    GLShader& operator=(const GLShader&) = delete;

    bool compile(GLenum type, const char* src) {
        destroy();
        id_ = glCreateShader(type);
        glShaderSource(id_, 1, &src, nullptr);
        glCompileShader(id_);
        GLint ok = 0;
        glGetShaderiv(id_, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            GLint len = 0;
            glGetShaderiv(id_, GL_INFO_LOG_LENGTH, &len);
            std::vector<char> log(len);
            glGetShaderInfoLog(id_, len, nullptr, log.data());
            std::cerr << "Shader compile error:\n" << log.data() << std::endl; // congrats, you broke GLSL
            destroy();
            return false;
        }
        return true;
    }

    GLuint id() const { return id_; }
    explicit operator bool() const { return id_ != 0; }

private:
    void destroy() { if (id_) { glDeleteShader(id_); id_ = 0; } }
    GLuint id_ = 0;
};

// ────────────────────────────────────────────────────────────
// GLProgram — owns a linked program + caches uniform locations
// TODO: consider wrapping uniform setters in a templated helper
//       so we stop writing glUniform1f/glUniform3f/etc by hand
// ────────────────────────────────────────────────────────────
class GLProgram {
public:
    GLProgram() = default;
    ~GLProgram() { destroy(); }

    GLProgram(GLProgram&& o) noexcept
        : id_(o.id_), uniforms_(std::move(o.uniforms_)) { o.id_ = 0; }
    GLProgram& operator=(GLProgram&& o) noexcept {
        if (this != &o) {
            destroy();
            id_ = o.id_; o.id_ = 0;
            uniforms_ = std::move(o.uniforms_);
        }
        return *this;
    }
    GLProgram(const GLProgram&) = delete;
    GLProgram& operator=(const GLProgram&) = delete;

    // Link from two raw shader ids (does NOT take ownership of shaders)
    bool link(GLuint vs, GLuint fs) {
        destroy();
        id_ = glCreateProgram();
        glAttachShader(id_, vs);
        glAttachShader(id_, fs);
        glLinkProgram(id_);
        GLint ok = 0;
        glGetProgramiv(id_, GL_LINK_STATUS, &ok);
        if (!ok) {
            GLint len = 0;
            glGetProgramiv(id_, GL_INFO_LOG_LENGTH, &len);
            std::vector<char> log(len);
            glGetProgramInfoLog(id_, len, nullptr, log.data());
            std::cerr << "Program link error:\n" << log.data() << std::endl;
            destroy();
            return false;
        }
        return true;
    }

    // Convenience: compile vert+frag from source, link, return success
    bool build(const char* vertSrc, const char* fragSrc) {
        GLShader vs(GL_VERTEX_SHADER, vertSrc);
        GLShader fs(GL_FRAGMENT_SHADER, fragSrc);
        if (!vs || !fs) return false;
        return link(vs.id(), fs.id());
    }

    void use() const { glUseProgram(id_); }

    // Cached uniform lookup — first call does glGetUniformLocation, later ones return cached value
    GLint uniform(const char* name) {
        auto it = uniforms_.find(name);
        if (it != uniforms_.end()) return it->second;
        GLint loc = glGetUniformLocation(id_, name);
        uniforms_[name] = loc;
        return loc;
    }

    // Convenience uniform setters
    void set1i(const char* n, int v)          { glUniform1i(uniform(n), v); }
    void set1f(const char* n, float v)        { glUniform1f(uniform(n), v); }
    void set2f(const char* n, float a, float b) { glUniform2f(uniform(n), a, b); }
    void set3f(const char* n, float a, float b, float c) { glUniform3f(uniform(n), a, b, c); }

    GLuint id() const { return id_; }
    explicit operator bool() const { return id_ != 0; }

private:
    void destroy() {
        if (id_) { glDeleteProgram(id_); id_ = 0; }
        uniforms_.clear();
    }
    GLuint id_ = 0;
    std::unordered_map<std::string, GLint> uniforms_;
};

// ────────────────────────────────────────────────────────────
// GLTexture2D — owns a single GL_TEXTURE_2D
// ────────────────────────────────────────────────────────────
class GLTexture2D {
public:
    GLTexture2D() = default;
    ~GLTexture2D() { destroy(); }

    GLTexture2D(GLTexture2D&& o) noexcept : id_(o.id_) { o.id_ = 0; }
    GLTexture2D& operator=(GLTexture2D&& o) noexcept {
        if (this != &o) { destroy(); id_ = o.id_; o.id_ = 0; }
        return *this;
    }
    GLTexture2D(const GLTexture2D&) = delete;
    GLTexture2D& operator=(const GLTexture2D&) = delete;

    // Take ownership of an existing raw texture id
    void adopt(GLuint rawTex) { destroy(); id_ = rawTex; }

    void bind(GLenum unit = GL_TEXTURE0) const {
        glActiveTexture(unit);
        glBindTexture(GL_TEXTURE_2D, id_);
    }

    GLuint id() const { return id_; }
    explicit operator bool() const { return id_ != 0; }

private:
    void destroy() { if (id_) { glDeleteTextures(1, &id_); id_ = 0; } }
    GLuint id_ = 0;
};

// ────────────────────────────────────────────────────────────
// GLVertexArray — owns a VAO + its associated VBO
// ────────────────────────────────────────────────────────────
class GLVertexArray {
public:
    GLVertexArray() = default;
    ~GLVertexArray() { destroy(); }

    GLVertexArray(GLVertexArray&& o) noexcept
        : vao_(o.vao_), vbo_(o.vbo_) { o.vao_ = o.vbo_ = 0; }
    GLVertexArray& operator=(GLVertexArray&& o) noexcept {
        if (this != &o) { destroy(); vao_ = o.vao_; vbo_ = o.vbo_; o.vao_ = o.vbo_ = 0; }
        return *this;
    }
    GLVertexArray(const GLVertexArray&) = delete;
    GLVertexArray& operator=(const GLVertexArray&) = delete;

    void bind() const { glBindVertexArray(vao_); }
    void unbind() const { glBindVertexArray(0); }

    GLuint vao() const { return vao_; }
    GLuint vbo() const { return vbo_; }

    // Create a full-screen quad VAO (2-component positions, triangle strip)
    static GLVertexArray makeFullScreenQuad() {
        GLVertexArray va;
        float verts[] = { -1, -1,  1, -1,  -1, 1,  1, 1 };
        glGenVertexArrays(1, &va.vao_);
        glGenBuffers(1, &va.vbo_);
        glBindVertexArray(va.vao_);
        glBindBuffer(GL_ARRAY_BUFFER, va.vbo_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
        return va;
    }

    void drawQuad() const {
        bind();
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

private:
    void destroy() {
        if (vbo_) { glDeleteBuffers(1, &vbo_); vbo_ = 0; }
        if (vao_) { glDeleteVertexArrays(1, &vao_); vao_ = 0; }
    }
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
};
