#include <os.hpp>
#include <psp2/apputil.h>
#include <psp2/system_param.h>

namespace OS {
bool toExit = false;
bool loadedSettings = false;
std::string *customProjectsPath = nullptr;
} // namespace OS

std::string OS::getPlatform() {
    return "Vita";
}

bool OS::isEnhancedPlatform() {
    return false;
}

std::string OS::getFilesystemRootPrefix() {
    return "ux0:";
}

std::string OS::getConfigFolderLocation() {
    return getScratchFolderLocation();
}

std::string OS::getScratchFolderLocation() {
    const std::string custom = getCustomScratchFolderLocation();
    if (!custom.empty()) return custom;
    return getFilesystemRootPrefix() + "data/scratch-vita/";
}

std::string OS::getRomFSLocation() {
    return "";
}

bool OS::isOnline() {
    return false;
}

bool OS::initWifi() {
    return true;
}

void OS::deInitWifi() {
}

std::string OS::getUsername() {
    static SceChar8 usernameBuffer[SCE_SYSTEM_PARAM_USERNAME_MAXSIZE];

    int result = sceAppUtilSystemParamGetString(
        SCE_SYSTEM_PARAM_ID_USERNAME,
        usernameBuffer,
        sizeof(usernameBuffer));

    if (result >= 0) {
        return std::string(reinterpret_cast<char *>(usernameBuffer));
    }

    return "Player";
}