#include "shader_utils.hpp"

// If something in here is returning 0, a shader exploded somewhere above you.
// Good luck.

#include <fstream>
#include <iostream>
#include <vector>

std::string readFile(const char* path) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file) return "";
    std::string contents;
    file.seekg(0, std::ios::end);
    contents.resize(file.tellg());
    file.seekg(0, std::ios::beg);
    file.read(&contents[0], contents.size());
    file.close();
    return contents;
}

void printShaderLog(GLuint shader) {
    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint logLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
        std::vector<char> log(logLen);
        glGetShaderInfoLog(shader, logLen, nullptr, log.data());
        std::cerr << "Shader compile error:\n" << log.data() << std::endl;
    }
}

void printProgramLog(GLuint program) {
    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLint logLen = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLen);
        std::vector<char> log(logLen);
        glGetProgramInfoLog(program, logLen, nullptr, log.data());
        std::cerr << "Program link error:\n" << log.data() << std::endl;
    }
}

GLuint compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    printShaderLog(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

// TODO: add a shader hot-reload mechanism so we can tweak the fragment shader
//       without restarting the entire simulation
GLuint compileShaderFromFile(GLenum type, const char* path) {
    std::string src = readFile(path);
    if (src.empty()) {
        std::cerr << "Failed to read shader: " << path << std::endl;
        return 0;
    }
    std::cerr << "Loading shader: " << path << std::endl;
    return compileShader(type, src.c_str());
}
