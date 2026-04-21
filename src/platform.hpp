#pragma once
// ============================================================
// platform.hpp — Centralized OpenGL loader and platform helpers
//
// Include this BEFORE any other OpenGL-related header throughout
// the codebase. Never write raw #ifdef __APPLE__ / __linux__ GL
// include blocks elsewhere — add them here instead.
// ============================================================

// ── OpenGL includes ──────────────────────────────────────────
#if defined(__APPLE__)
    // macOS ships a full OpenGL 3.3–4.1 Core Profile via the system
    // framework. No external loader is needed.
    #ifndef GL_SILENCE_DEPRECATION
        #define GL_SILENCE_DEPRECATION
    #endif
    #include <OpenGL/gl3.h>

    // No-op: context is already Core Profile, all symbols are present.
    inline void platformInitGL() {}

#elif defined(__linux__) || defined(__unix__)
    // On Linux (X11/Wayland-via-XWayland, Mesa, NVIDIA, AMD) we need GLEW
    // to expose OpenGL 3+ entry points at runtime.
    // GLEW must be included before any other GL header.
    #include <GL/glew.h>
    #include <cstdio>

    // Call once, immediately after the first OpenGL context becomes current.
    inline void platformInitGL() {
        glewExperimental = GL_TRUE;
        const GLenum err = glewInit();
        if (err != GLEW_OK) {
            std::fprintf(stderr, "[platform] GLEW init failed: %s\n",
                         glewGetErrorString(err));
        }
        // GLEW often raises a spurious GL_INVALID_ENUM on init — discard it.
        glGetError();
    }

#elif defined(_WIN32)
    // Future Windows support: swap in GLEW, GLAD, or WGL as needed.
    #include <GL/glew.h>
    #include <cstdio>
    inline void platformInitGL() {
        glewExperimental = GL_TRUE;
        const GLenum err = glewInit();
        if (err != GLEW_OK) {
            std::fprintf(stderr, "[platform] GLEW init failed: %s\n",
                         glewGetErrorString(err));
        }
        glGetError();
    }

#else
    #error "Unsupported platform. Add an OpenGL loader for your platform in src/platform.hpp."
#endif

// ── Executable directory helper ──────────────────────────────
// Returns the directory containing the running executable as a
// null-terminated string written into `out` (max `outSize` bytes).
// Returns true on success, false on failure.
#include <cstdint>
#include <cstring>

#if defined(__APPLE__)
    #include <libproc.h>
    #include <unistd.h>
    #include <libgen.h>
    inline bool platformExeDir(char* out, std::size_t outSize) {
        char buf[PROC_PIDPATHINFO_MAXSIZE];
        int ret = proc_pidpath(getpid(), buf, sizeof(buf));
        if (ret <= 0) return false;
        const char* dir = dirname(buf);
        std::strncpy(out, dir, outSize - 1);
        out[outSize - 1] = '\0';
        return true;
    }
#elif defined(__linux__) || defined(__unix__)
    #include <unistd.h>
    inline bool platformExeDir(char* out, std::size_t outSize) {
        char buf[4096];
        const ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
        if (len == -1) return false;
        buf[len] = '\0';
        // Strip the filename component manually (avoid basename/dirname mutability issues).
        char* slash = buf + len;
        while (slash > buf && *slash != '/') --slash;
        const std::size_t dirLen = static_cast<std::size_t>(slash - buf);
        if (dirLen == 0) { out[0] = '.'; out[1] = '\0'; return true; }
        if (dirLen >= outSize) return false;
        std::memcpy(out, buf, dirLen);
        out[dirLen] = '\0';
        return true;
    }
#elif defined(_WIN32)
    #include <windows.h>
    inline bool platformExeDir(char* out, std::size_t outSize) {
        char buf[MAX_PATH];
        const DWORD len = GetModuleFileNameA(nullptr, buf, MAX_PATH);
        if (len == 0) return false;
        char* slash = buf + len;
        while (slash > buf && *slash != '\\' && *slash != '/') --slash;
        const std::size_t dirLen = static_cast<std::size_t>(slash - buf);
        if (dirLen >= outSize) return false;
        std::memcpy(out, buf, dirLen);
        out[dirLen] = '\0';
        return true;
    }
#else
    inline bool platformExeDir(char* out, std::size_t outSize) {
        if (outSize > 0) { out[0] = '.'; out[1] = '\0'; }
        return false;
    }
#endif
