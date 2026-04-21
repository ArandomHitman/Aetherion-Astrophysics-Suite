#pragma once
// ============================================================
// resource_manager.hpp — Asset path resolution
// ============================================================

#include "platform.hpp"   // provides platformExeDir()
#include <string>
#include <vector>
#include <fstream>
#include <iostream>

class ResourceManager {
public:
    ResourceManager() {
        // Default search paths (in priority order)
        searchPaths_ = { ".", "build", "..", "../build", "../..", "../../build", "src", "../src", "../../src" };

        // Prepend the directory of the running executable so that shaders and
        // textures are found regardless of the working directory.
        // On macOS this also surfaces the .app bundle's Resources folder.
        char exeDir[4096];
        if (platformExeDir(exeDir, sizeof(exeDir))) {
            std::string dir(exeDir);
            // Insert exe dir first, then the macOS-specific Resources path.
            searchPaths_.insert(searchPaths_.begin(), dir);
#if defined(__APPLE__)
            // Inside a .app bundle: Contents/MacOS/../Resources
            searchPaths_.insert(searchPaths_.begin(), dir + "/../Resources");
#endif
        }

#if defined(__linux__) || defined(__unix__)
        // XDG data directories — populated by Flatpak (/app/share:/usr/share)
        // and by regular system/user installs.  Appended after the exe-dir so
        // development builds (where the binary sits next to the assets) still
        // take priority over installed data.
        const char* xdgDataDirs = std::getenv("XDG_DATA_DIRS");
        if (xdgDataDirs && xdgDataDirs[0] != '\0') {
            std::string dirs(xdgDataDirs);
            std::string::size_type start = 0, end;
            while ((end = dirs.find(':', start)) != std::string::npos) {
                searchPaths_.push_back(dirs.substr(start, end - start) + "/aetherionsuite");
                start = end + 1;
            }
            searchPaths_.push_back(dirs.substr(start) + "/aetherionsuite");
        }
        // Fallback for installs that predate XDG env setup.
        searchPaths_.push_back("/usr/local/share/aetherionsuite");
        searchPaths_.push_back("/usr/share/aetherionsuite");
#endif
    }

    // TODO: add a way to mount virtual paths (e.g. "shaders://" → resolved dir)
    //       so callers don't need to know the physical layout
    explicit ResourceManager(std::vector<std::string> paths)
        : searchPaths_(std::move(paths)) {}

    void addSearchPath(const std::string& path) {
        searchPaths_.push_back(path);
    }

    // Find the first existing file matching `filename` across search paths.
    // Returns the full relative path, or empty string if not found.
    // ("not found" means you've moved an asset again and forgot to update CMake. you know who you are.)
    std::string find(const std::string& filename) const {
        for (const auto& dir : searchPaths_) {
            std::string candidate = dir + "/" + filename;
            std::ifstream f(candidate);
            if (f.good()) return candidate;
        }
        // Also try filename directly (in case it's an absolute or already correct relative path)
        std::ifstream f(filename);
        if (f.good()) return filename;
        return {};
    }

    // Return a human-readable list of search paths (for error messages).
    std::string paths() const {
        std::string result;
        for (size_t i = 0; i < searchPaths_.size(); ++i) {
            if (i) result += ", ";
            result += searchPaths_[i];
        }
        return result;
    }

    // Find a file, logging the result. Returns empty string if not found.
    std::string findAndLog(const std::string& filename) const {
        std::string result = find(filename);
        if (!result.empty()) {
            std::cerr << "Found asset: " << result << std::endl;
        } else {
            std::cerr << "Asset not found: " << filename << std::endl;
        }
        return result;
    }

private:
    std::vector<std::string> searchPaths_;
};
