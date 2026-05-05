#include <os.hpp>

extern char nickname[0x21];

namespace OS {
bool toExit = false;
bool loadedSettings = false;
std::string *customProjectsPath = nullptr;
} // namespace OS

std::string OS::getPlatform() {
    return "Switch";
}

bool OS::isEnhancedPlatform() {
    return false;
}

std::string OS::getFilesystemRootPrefix() {
    return "";
}

std::string OS::getConfigFolderLocation() {
    return getScratchFolderLocation();
}

std::string OS::getScratchFolderLocation() {
    const std::string custom = getCustomScratchFolderLocation();
    if (!custom.empty()) return custom;
    return "/switch/scratch-nx/";
}

std::string OS::getRomFSLocation() {
    return "romfs:/";
}

bool OS::isOnline() {
    // TODO: Add an actual way to check if online
#if defined(ENABLE_DOWNLOAD) || defined(ENABLE_CLOUDVARS)
    return true;
#endif
    return false;
}

bool OS::initWifi() {
    return true;
}

void OS::deInitWifi() {
}

std::string OS::getUsername() {
    if (std::string(nickname) != "") {
        return std::string(nickname);
    }
    return "Player";
}