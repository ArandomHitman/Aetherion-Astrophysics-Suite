/*------------------------------------------------------
    launcher.hpp
    The main menu for Aetherion - Astrophysics Suite 
    Handles locating and launching sibling executables (blackhole-2D, blackhole-3D), alongside 
    settings, preset loading, and other shared utilities. This is a header-only module for simplicity, 
    included directly from the same directory as the main-menu binary.
------------------------------------------------------*/
#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <cstdlib>
#include <iostream>
#include <csignal>

#if defined(_WIN32)
    #include <windows.h>
#else
    #ifdef __APPLE__
    #  include <mach-o/dyld.h>
    #endif
    #include <unistd.h>
    #include <sys/wait.h>
#endif

namespace launcher {

// Tracks PIDs / HANDLEs of launched child simulator processes.
#if defined(_WIN32)
inline std::vector<HANDLE>& launchedHandles() {
    static std::vector<HANDLE> handles;
    return handles;
}
#else
inline std::vector<pid_t>& launchedPids() {
    static std::vector<pid_t> pids;
    return pids; 
}
#endif

// Returns the directory containing the currently running executable.
inline std::filesystem::path executableDir() {
#ifdef __APPLE__
    char buf[4096];
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) == 0) {
        return std::filesystem::canonical(buf).parent_path();
    }
#elif defined(__linux__)
    auto p = std::filesystem::read_symlink("/proc/self/exe");
    return p.parent_path();
#endif
    // Fallback: assume current working directory
    return std::filesystem::current_path();
}

// Launches a sibling executable by name, optionally passing extra arguments.
// The child runs asynchronously (detached) so the menu stays open.
// Tracks the child PID (POSIX) or HANDLE (Windows) for later cleanup.
inline bool launchSimulator(const std::string& execName,
                            const std::string& args = "") {
    auto dir = executableDir();
    auto path = dir / execName;

    if (!std::filesystem::exists(path)) {
        std::cerr << "Error: Could not find executable: " << path << "\n";
        return false;
    }

#if defined(_WIN32)
    // Windows: use CreateProcessA to launch the child.
    std::string cmdLine = "\"" + path.string() + "\"";
    if (!args.empty()) cmdLine += " " + args;

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessA(
        nullptr,
        const_cast<char*>(cmdLine.c_str()),
        nullptr, nullptr,
        FALSE, 0, nullptr, nullptr,
        &si, &pi);
    if (!ok) {
        std::cerr << "Error: CreateProcess failed (" << GetLastError() << ") for " << path << "\n";
        return false;
    }
    CloseHandle(pi.hThread);
    launchedHandles().push_back(pi.hProcess);
    return true;
#else
    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "Error: fork() failed for " << path << "\n";
        return false;
    }
    if (pid == 0) {
        // Child process — exec the simulator
        if (args.empty()) {
            execl(path.string().c_str(), execName.c_str(), (char*)nullptr);
        } else {
            execl("/bin/sh", "sh", "-c",
                  ("\"" + path.string() + "\" " + args).c_str(),
                  (char*)nullptr);
        }
        // exec failed, skill issue
        _exit(127);
    }

    // Parent — track the child PID
    launchedPids().push_back(pid);
    return true;
#endif
}

// Terminates all tracked child simulators and releases resources.
inline void killAllSimulators() {
#if defined(_WIN32)
    for (HANDLE h : launchedHandles()) {
        TerminateProcess(h, 0);
        WaitForSingleObject(h, 1000);
        CloseHandle(h);
    }
    launchedHandles().clear();
#else
    for (pid_t pid : launchedPids()) {
        // Check if the process is still alive
        if (kill(pid, 0) == 0) {
            kill(pid, SIGTERM);
        }
    }
    // Brief wait for graceful shutdown, then reap
    for (pid_t pid : launchedPids()) {
        int status = 0;
        waitpid(pid, &status, WNOHANG);
    }
    launchedPids().clear();
#endif
}

} // namespace launcher