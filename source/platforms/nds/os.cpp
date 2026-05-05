#include <nds.h>
#include <os.hpp>

namespace OS {
bool toExit = false;
bool loadedSettings = false;
std::string *customProjectsPath = nullptr;
} // namespace OS

std::string OS::getPlatform() {
    return "DS";
}

bool OS::isEnhancedPlatform() {
    return isDSiMode();
}

std::string OS::getFilesystemRootPrefix() {
    return "fat:";
}

std::string OS::getConfigFolderLocation() {
    return getScratchFolderLocation();
}

std::string OS::getScratchFolderLocation() {
    const std::string custom = getCustomScratchFolderLocation();
    if (!custom.empty()) return custom;
    return getFilesystemRootPrefix() + "/scratch-ds/";
}

std::string OS::getRomFSLocation() {
    return "nitro:/";
}

bool OS::isOnline() {
    return false;
}

bool OS::initWifi() {
    return false;
}

void OS::deInitWifi() {
}

std::string OS::getUsername() {
    // TODO: Implement
    return "Player";
}