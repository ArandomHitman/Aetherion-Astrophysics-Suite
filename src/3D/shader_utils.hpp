#pragma once

#include "platform.hpp"

#include <string>

// Read a file into a string
std::string readFile(const char* path);

// Print shader compilation errors to stderr
void printShaderLog(GLuint shader);

// Print program link errors to stderr
void printProgramLog(GLuint program);

// Compile a shader from source string; returns 0 on failure
GLuint compileShader(GLenum type, const char* src);

// Compile a shader from a file path; returns 0 on failure
GLuint compileShaderFromFile(GLenum type, const char* path);
