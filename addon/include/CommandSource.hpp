#pragma once

#include "MenuLink.hpp"
#include "SearchEngine.hpp"
#include <windows.h>
#include <set>
#include <string>
#include <vector>

namespace Baaz {

// The Win32 menu bar of a frame window. Menus are filled the way Windows does before showing
// them: WM_INITMENU once, then WM_INITMENUPOPUP for each level, top-down.
class LiveMenu : public MenuSource {
public:
    explicit LiveMenu(HWND frame);
    std::vector<std::wstring> TopLevelTitles() override;
    std::vector<LiveEntry> Entries(const std::vector<int>& path) override;

private:
    HMENU MenuBar();
    HMENU FilledMenu(const std::vector<int>& path);

    HWND m_frame;
    bool m_barInitialized = false;
    std::set<std::vector<int>> m_filled;
};

// Archicad's menu commands, found by the names in the user's exported command scheme.
class CommandSource {
public:
    // Reads %USERPROFILE%\.baaz\commands\*.xml (re-read only when a file time changes), picks the
    // export that fits the live menu bar best, checks that Archicad has not saved a newer scheme,
    // and links. Without a usable export the result is a single Help line, or empty without any export.
    static std::vector<SearchItem> BuildCommandIndex(HWND frame, const std::vector<DynamicList>& lists);
    // Fills the item's menu path again and posts WM_COMMAND only if the live item is still an
    // enabled item with the same id.
    static bool Run(const SearchItem& item, HWND frame);

    struct CachedExport {
        std::wstring path;
        FILETIME time = {};
        std::vector<MenuNode> bar;
    };

    // Where the exports, the scheme files Archicad saves and the running program are
    struct Locations {
        std::wstring commands;
        std::wstring graphisoft;
        std::wstring modulePath;
    };

#ifndef ACAPI_VERSION
    // Replaces the commands folder, %APPDATA% and the path of the running program, and forgets
    // everything read so far. The default module path is that of an Archicad 29 installation.
    static void SetFoldersForTest(std::wstring commands, std::wstring appData,
                                  std::wstring modulePath = L"C:\\Test\\Archicad 29\\ARCHICAD.exe");
#endif

private:
    static Locations CurrentLocations();
    static const std::vector<CachedExport>& LoadExports(const std::wstring& folder);

    static std::vector<CachedExport> s_exports;
    static bool s_checkNoted;
#ifndef ACAPI_VERSION
    static Locations s_testLocations;
#endif
};

} // namespace Baaz
