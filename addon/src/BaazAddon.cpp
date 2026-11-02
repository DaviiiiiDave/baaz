#include "APIEnvir.h"
#include "ACAPinc.h"
#include "RS.hpp"

#include "BaazAddon.hpp"
#include "HudWindow.hpp"
#include "MessageHook.hpp"
#include "SearchEngine.hpp"
#include "Settings.hpp"

namespace Baaz {

static HWND s_mainWindow = NULL;

HINSTANCE AddonInstance() {
    HMODULE hModule = NULL;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&AddonInstance), &hModule);
    return hModule;
}

HWND MainWindow() {
    return s_mainWindow;
}

} // namespace Baaz

// Archicad add-on entry points

API_AddonType CheckEnvironment(API_EnvirParams* envir) {
    RSGetIndString(&envir->addOnInfo.name, 32000, 1, ACAPI_GetOwnResModule());
    RSGetIndString(&envir->addOnInfo.description, 32000, 2, ACAPI_GetOwnResModule());
    return APIAddon_Preload;
}

GSErrCode RegisterInterface(void) {
    return NoError;
}

GSErrCode Initialize(void) {
    Baaz::s_mainWindow = static_cast<HWND>(ACAPI_GetMainWindow());

    Baaz::Settings::Load();
    Baaz::SearchEngine::Initialize();
    Baaz::HudWindow::Initialize(Baaz::AddonInstance());
    Baaz::MessageHook::Install(Baaz::s_mainWindow);
    return NoError;
}

GSErrCode FreeData(void) {
    Baaz::MessageHook::Uninstall();
    Baaz::HudWindow::Shutdown();
    Baaz::SearchEngine::ClearProjectItems();
    return NoError;
}
