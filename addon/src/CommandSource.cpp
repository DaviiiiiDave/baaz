#include "CommandSource.hpp"
#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string_view>
#include <system_error>
#include <utility>

namespace Baaz {

namespace fs = std::filesystem;

static const wchar_t* const kOutOfDate = L"Command list is out of date. Export the command scheme again (see README).";
static const wchar_t* const kCannotCheck = L"Command list: cannot check whether the export is current";
static const std::string_view kSchemeMarker = "6FCD168F-A26C-4547-AF9D-ED9A5FC54DD7";
static const size_t kMarkerBytes = 512;
static const int kMaxSchemeDepth = 3;
// The id of WM_COMMAND is 16 bits wide
static const unsigned kMaxCommandId = 0xFFFF;

std::vector<CommandSource::CachedExport> CommandSource::s_exports;
bool CommandSource::s_checkNoted = false;

// --- LiveMenu ------------------------------------------------------------------------------

LiveMenu::LiveMenu(HWND frame) : m_frame(frame) {}

HMENU LiveMenu::MenuBar() {
    HMENU bar = GetMenu(m_frame);
    if (bar != nullptr && !m_barInitialized) {
        SendMessageW(m_frame, WM_INITMENU, reinterpret_cast<WPARAM>(bar), 0);
        m_barInitialized = true;
    }
    return bar;
}

// The popup at path, with every popup on the way filled once per LiveMenu
HMENU LiveMenu::FilledMenu(const std::vector<int>& path) {
    HMENU menu = MenuBar();
    std::vector<int> prefix;
    for (int index : path) {
        if (menu == nullptr) return nullptr;
        menu = GetSubMenu(menu, index);
        prefix.push_back(index);
        if (menu != nullptr && m_filled.insert(prefix).second) {
            SendMessageW(m_frame, WM_INITMENUPOPUP, reinterpret_cast<WPARAM>(menu), MAKELPARAM(index, 0));
        }
    }
    return menu;
}

static std::wstring MenuTitle(HMENU bar, int index) {
    const int length = GetMenuStringW(bar, static_cast<UINT>(index), nullptr, 0, MF_BYPOSITION);
    if (length <= 0) return L"";
    std::wstring title(static_cast<size_t>(length) + 1, wchar_t());
    const int copied = GetMenuStringW(bar, static_cast<UINT>(index), title.data(), length + 1, MF_BYPOSITION);
    title.resize(copied > 0 ? static_cast<size_t>(copied) : 0);
    return title;
}

std::vector<std::wstring> LiveMenu::TopLevelTitles() {
    std::vector<std::wstring> titles;
    HMENU bar = MenuBar();
    const int count = bar != nullptr ? GetMenuItemCount(bar) : 0;
    for (int i = 0; i < count; ++i) titles.push_back(CleanName(MenuTitle(bar, i)));
    return titles;
}

static MenuNodeType EntryType(const MENUITEMINFOW& info) {
    if ((info.fType & MFT_SEPARATOR) != 0) return MenuNodeType::Separator;
    return info.hSubMenu != nullptr ? MenuNodeType::Popup : MenuNodeType::Item;
}

static bool ReadEntry(HMENU menu, int index, LiveEntry& entry) {
    MENUITEMINFOW info = {};
    info.cbSize = sizeof(info);
    info.fMask = MIIM_FTYPE | MIIM_ID | MIIM_STATE | MIIM_SUBMENU;
    if (!GetMenuItemInfoW(menu, static_cast<UINT>(index), TRUE, &info)) return false;
    entry.type = EntryType(info);
    entry.liveId = entry.type == MenuNodeType::Item ? info.wID : 0;
    entry.enabled = (info.fState & (MFS_GRAYED | MFS_DISABLED)) == 0;
    return true;
}

std::vector<LiveEntry> LiveMenu::Entries(const std::vector<int>& path) {
    std::vector<LiveEntry> entries;
    HMENU menu = FilledMenu(path);
    const int count = menu != nullptr ? GetMenuItemCount(menu) : 0;
    for (int i = 0; i < count; ++i) {
        LiveEntry entry;
        // A menu that cannot be read completely is not linked at all
        if (!ReadEntry(menu, i, entry)) return {};
        entries.push_back(entry);
    }
    return entries;
}

// --- Files ---------------------------------------------------------------------------------

static std::wstring EnvironmentValue(const wchar_t* name) {
    wchar_t buffer[2048];
    const DWORD length = GetEnvironmentVariableW(name, buffer, static_cast<DWORD>(std::size(buffer)));
    return (length > 0 && length < std::size(buffer)) ? std::wstring(buffer, length) : std::wstring();
}

static std::wstring RunningModulePath() {
    wchar_t buffer[2048];
    const DWORD length = GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
    return (length > 0 && length < std::size(buffer)) ? std::wstring(buffer, length) : std::wstring();
}

CommandSource::Locations CommandSource::CurrentLocations() {
#ifndef ACAPI_VERSION
    if (!s_testLocations.commands.empty()) return s_testLocations;
#endif
    Locations locations;
    locations.modulePath = RunningModulePath();
    const std::wstring userProfile = EnvironmentValue(L"USERPROFILE");
    if (!userProfile.empty()) locations.commands = (fs::path(userProfile) / L".baaz" / L"commands").wstring();
    const std::wstring appData = EnvironmentValue(L"APPDATA");
    if (!appData.empty()) locations.graphisoft = (fs::path(appData) / L"GRAPHISOFT").wstring();
    return locations;
}

#ifndef ACAPI_VERSION
CommandSource::Locations CommandSource::s_testLocations;

void CommandSource::SetFoldersForTest(std::wstring commands, std::wstring appData, std::wstring modulePath) {
    s_testLocations = { std::move(commands), (fs::path(appData) / L"GRAPHISOFT").wstring(), std::move(modulePath) };
    s_exports.clear();
    s_checkNoted = false;
}
#endif

static bool HasExtension(const fs::path& path, const wchar_t* extension) {
    return _wcsicmp(path.extension().c_str(), extension) == 0;
}

static bool ReadWriteTime(const fs::path& path, FILETIME& time) {
    WIN32_FILE_ATTRIBUTE_DATA data = {};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) return false;
    time = data.ftLastWriteTime;
    return true;
}

static std::vector<CommandSource::CachedExport> ListExports(const std::wstring& folder) {
    std::vector<CommandSource::CachedExport> exports;
    std::error_code error;
    for (fs::directory_iterator it(folder, error), end; !error && it != end; it.increment(error)) {
        CommandSource::CachedExport found;
        if (it->is_regular_file(error) && HasExtension(it->path(), L".xml") && ReadWriteTime(it->path(), found.time)) {
            found.path = it->path().wstring();
            exports.push_back(std::move(found));
        }
    }
    std::sort(exports.begin(), exports.end(),
              [](const CommandSource::CachedExport& a, const CommandSource::CachedExport& b) { return a.path < b.path; });
    return exports;
}

static bool SameFiles(const std::vector<CommandSource::CachedExport>& a, const std::vector<CommandSource::CachedExport>& b) {
    return std::equal(a.begin(), a.end(), b.begin(), b.end(),
                      [](const CommandSource::CachedExport& x, const CommandSource::CachedExport& y) {
                          return x.path == y.path && CompareFileTime(&x.time, &y.time) == 0;
                      });
}

static std::vector<MenuNode> ReadBar(const std::wstring& path) {
    std::ifstream file(fs::path(path), std::ios::binary);
    const std::string xml((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return SchemeReader::ReadFloorPlanMenuBar(xml);
}

const std::vector<CommandSource::CachedExport>& CommandSource::LoadExports(const std::wstring& folder) {
    std::vector<CachedExport> listed = ListExports(folder);
    if (SameFiles(listed, s_exports)) return s_exports;
    for (CachedExport& found : listed) found.bar = ReadBar(found.path);
    s_exports = std::move(listed);
    return s_exports;
}

// The export that links most commands; none if no export fits or two fit equally well
static const CommandSource::CachedExport* BestExport(const std::vector<CommandSource::CachedExport>& exports,
                                                     MenuSource& live) {
    const CommandSource::CachedExport* best = nullptr;
    int bestScore = -1;
    bool tie = false;
    for (const CommandSource::CachedExport& candidate : exports) {
        const int score = MenuLink::FitScore(candidate.bar, live);
        if (score > bestScore) {
            best = &candidate;
            bestScore = score;
            tie = false;
        } else if (score == bestScore && score >= 0) {
            tie = true;
        }
    }
    return tie ? nullptr : best;
}

// --- Staleness -----------------------------------------------------------------------------

enum class Freshness { Current, Stale, Unknown };

// "29" for ...\Archicad 29\ARCHICAD.exe; empty if the program does not sit in such a folder
static std::wstring ArchicadMajorVersion(const std::wstring& modulePath) {
    const std::wstring folder = fs::path(modulePath).parent_path().filename().wstring();
    const std::wstring prefix = L"Archicad ";
    if (folder.compare(0, prefix.size(), prefix) != 0) return L"";
    size_t end = prefix.size();
    while (end < folder.size() && iswdigit(folder[end])) ++end;
    return folder.substr(prefix.size(), end - prefix.size());
}

// "Archicad 29 DEU R1" belongs to version 29, "Archicad 290" does not
static bool IsFolderOfVersion(const std::wstring& name, const std::wstring& major) {
    const std::wstring prefix = L"Archicad " + major;
    if (name.compare(0, prefix.size(), prefix) != 0) return false;
    return name.size() == prefix.size() || !iswdigit(name[prefix.size()]);
}

static std::vector<fs::path> ArchicadFolders(const std::wstring& graphisoft, const std::wstring& major) {
    std::vector<fs::path> folders;
    std::error_code error;
    for (fs::directory_iterator it(graphisoft, error), end; !error && it != end; it.increment(error)) {
        if (it->is_directory(error) && IsFolderOfVersion(it->path().filename().wstring(), major)) {
            folders.push_back(it->path());
        }
    }
    return folders;
}

static bool HasSchemeMarker(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    char head[kMarkerBytes];
    file.read(head, sizeof(head));
    return std::string_view(head, static_cast<size_t>(file.gcount())).find(kSchemeMarker) != std::string_view::npos;
}

// false if the folder could not be read completely: a newer scheme file may be in the part not read
static bool CollectSchemeTimes(const fs::path& folder, std::vector<FILETIME>& times) {
    std::error_code error;
    fs::recursive_directory_iterator it(folder, error);
    for (; !error && it != fs::recursive_directory_iterator(); it.increment(error)) {
        if (it.depth() >= kMaxSchemeDepth) it.disable_recursion_pending();
        std::error_code entryError;
        if (!it->is_regular_file(entryError) || !HasExtension(it->path(), L".bin") || !HasSchemeMarker(it->path())) continue;
        FILETIME time;
        if (!ReadWriteTime(it->path(), time)) return false;
        times.push_back(time);
    }
    return !error;
}

static Freshness CheckFreshness(const FILETIME& exportTime, const CommandSource::Locations& where) {
    const std::wstring major = ArchicadMajorVersion(where.modulePath);
    const std::vector<fs::path> folders = major.empty() ? std::vector<fs::path>() : ArchicadFolders(where.graphisoft, major);
    if (folders.empty()) return Freshness::Unknown;
    std::vector<FILETIME> schemeTimes;
    for (const fs::path& folder : folders) {
        if (!CollectSchemeTimes(folder, schemeTimes)) return Freshness::Unknown;
    }
    // No scheme file found is no evidence that the export is current
    if (schemeTimes.empty()) return Freshness::Unknown;
    return MenuLink::IsExportStale(exportTime, schemeTimes) ? Freshness::Stale : Freshness::Current;
}

// --- CommandSource -------------------------------------------------------------------------

static SearchItem StatusItem(const wchar_t* text) {
    SearchItem item;
    item.title = text;
    item.kind = ItemKind::Help;
    return item;
}

static std::vector<SearchItem> ToSearchItems(const std::vector<LinkedCommand>& linked) {
    std::vector<SearchItem> items;
    for (const LinkedCommand& command : linked) {
        SearchItem item;
        item.title = command.name;
        item.tag = command.menuPath;
        item.kind = ItemKind::Command;
        item.menuPosition = command.position;
        item.liveId = command.liveId;
        item.enabled = command.enabled;
        items.push_back(std::move(item));
    }
    return items;
}

std::vector<SearchItem> CommandSource::BuildCommandIndex(HWND frame, const std::vector<DynamicList>& lists) {
    if (!IsWindow(frame)) return {};
    const Locations where = CurrentLocations();
    const std::vector<CachedExport>& exports = LoadExports(where.commands);
    if (exports.empty()) return {};
    LiveMenu live(frame);
    const CachedExport* chosen = BestExport(exports, live);
    if (chosen == nullptr) return { StatusItem(kOutOfDate) };
    const Freshness freshness = CheckFreshness(chosen->time, where);
    if (freshness == Freshness::Stale) return { StatusItem(kOutOfDate) };
    std::vector<SearchItem> items = ToSearchItems(MenuLink::Link(chosen->bar, live, lists));
    if (freshness == Freshness::Unknown && !s_checkNoted) {
        s_checkNoted = true;
        items.push_back(StatusItem(kCannotCheck));
    }
    return items;
}

static bool HasRunnableShape(const SearchItem& item) {
    return item.kind == ItemKind::Command && item.menuPosition.size() >= 2 && item.liveId != 0 &&
           item.liveId <= kMaxCommandId && item.menuPosition.back() >= 0;
}

bool CommandSource::Run(const SearchItem& item, HWND frame) {
    if (!HasRunnableShape(item) || !IsWindow(frame)) return false;
    LiveMenu live(frame);
    const std::vector<int> menuPath(item.menuPosition.begin(), item.menuPosition.end() - 1);
    const std::vector<LiveEntry> entries = live.Entries(menuPath);
    const size_t index = static_cast<size_t>(item.menuPosition.back());
    if (index >= entries.size()) return false;
    const LiveEntry& entry = entries[index];
    if (entry.type != MenuNodeType::Item || !entry.enabled || entry.liveId != item.liveId) return false;
    return PostMessageW(frame, WM_COMMAND, MAKEWPARAM(item.liveId, 0), 0) != FALSE;
}

} // namespace Baaz
