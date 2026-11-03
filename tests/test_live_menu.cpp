// Keep assert() active in Release builds too
#undef NDEBUG
#include "CommandSource.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace Baaz;

// --- A real window with a Win32 menu bar whose popup is filled on demand -----------------

// Normal: Alpha 255, separator, Beta 300. Shifted: one more active entry in front. ExtraItem: one more
// entry in each segment. Nested: a submenu "Sub" at index 3 that is filled in its own WM_INITMENUPOPUP.
enum class Fill { Normal, Grayed, ChangedId, Shifted, ExtraItem, Nested };

static Fill g_fill = Fill::Normal;
static HMENU g_popup = nullptr;
static HMENU g_sub = nullptr;
static std::vector<unsigned> g_commands;

static void ClearMenu(HMENU menu) {
    while (GetMenuItemCount(menu) > 0) RemoveMenu(menu, 0, MF_BYPOSITION);
}

static void FillPopup(HMENU popup) {
    ClearMenu(popup);
    if (g_fill == Fill::Shifted) AppendMenuW(popup, MF_STRING, 400, L"Shifted");
    UINT firstFlags = g_fill == Fill::Grayed ? (MF_STRING | MF_GRAYED) : MF_STRING;
    UINT firstId = g_fill == Fill::ChangedId ? 256 : 255;
    AppendMenuW(popup, firstFlags, firstId, L"A&lpha\tCtrl+A");
    if (g_fill == Fill::ExtraItem) AppendMenuW(popup, MF_STRING, 401, L"Extra");
    AppendMenuW(popup, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(popup, MF_STRING, 300, L"Beta");
    if (g_fill == Fill::ExtraItem) AppendMenuW(popup, MF_STRING, 402, L"Extra");
    if (g_fill == Fill::Nested) {
        if (g_sub == nullptr) g_sub = CreatePopupMenu();
        AppendMenuW(popup, MF_POPUP, reinterpret_cast<UINT_PTR>(g_sub), L"Sub");
    }
}

static void FillSub(HMENU sub) {
    ClearMenu(sub);
    AppendMenuW(sub, MF_STRING, 500, L"S1");
    AppendMenuW(sub, MF_STRING, 501, L"S2");
}

static LRESULT CALLBACK FrameProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_INITMENUPOPUP && reinterpret_cast<HMENU>(wParam) == g_popup) {
        FillPopup(g_popup);
        return 0;
    }
    if (msg == WM_INITMENUPOPUP && g_sub != nullptr && reinterpret_cast<HMENU>(wParam) == g_sub) {
        FillSub(g_sub);
        return 0;
    }
    if (msg == WM_COMMAND) {
        g_commands.push_back(LOWORD(wParam));
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

class TestFrame {
public:
    // withOtherMenus: a menu "Extra" in front of "Test" and an untitled slot after it, as in a real bar
    explicit TestFrame(Fill fill, bool withOtherMenus = false) {
        g_fill = fill;
        g_sub = nullptr;
        g_commands.clear();
        static const wchar_t* const kClass = L"BaazLiveMenuTestFrame";
        WNDCLASSW windowClass = {};
        windowClass.lpfnWndProc = FrameProc;
        windowClass.hInstance = GetModuleHandleW(nullptr);
        windowClass.lpszClassName = kClass;
        RegisterClassW(&windowClass);
        HMENU bar = CreateMenu();
        g_popup = CreatePopupMenu();
        if (withOtherMenus) {
            HMENU extra = CreatePopupMenu();
            AppendMenuW(extra, MF_STRING, 700, L"Other");
            AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(extra), L"&Extra");
        }
        AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(g_popup), L"&Test");
        if (withOtherMenus) AppendMenuW(bar, MF_STRING, 701, L"");
        m_hwnd = CreateWindowExW(0, kClass, L"frame", WS_OVERLAPPEDWINDOW, 0, 0, 200, 100, nullptr, bar,
                                 windowClass.hInstance, nullptr);
        assert(m_hwnd != nullptr);
    }
    ~TestFrame() {
        DestroyWindow(m_hwnd);
        if (g_sub != nullptr && IsMenu(g_sub)) DestroyMenu(g_sub);
        g_popup = nullptr;
        g_sub = nullptr;
    }
    TestFrame(const TestFrame&) = delete;
    TestFrame& operator=(const TestFrame&) = delete;

    HWND Hwnd() const { return m_hwnd; }
    void ChangeFill(Fill fill) { g_fill = fill; }

private:
    HWND m_hwnd = nullptr;
};

static void Pump() {
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
}

static SearchItem CommandItem(unsigned liveId, std::vector<int> position) {
    SearchItem item;
    item.kind = ItemKind::Command;
    item.liveId = liveId;
    item.menuPosition = std::move(position);
    return item;
}

// --- Temporary commands folder and %APPDATA% root ------------------------------------------

static const char* const kSchemeMarker = "6FCD168F-A26C-4547-AF9D-ED9A5FC54DD7";
static const char* const kExportXml =
    "<Scheme><GlobalSettings><CommandLayout>"
    "<MenuBar ID=\"B\" menubarguid=\"2085E4D3-5694-4BB9-ACAC-3AED5BE50091\" name=\"x\">"
    "<Menu ID=\"M\" title=\"&amp;Test\">"
    "<Item command=\"1-0x0-0x0\" comment=\"Alpha\"/><Separator/><Item command=\"2-0x0-0x0\" comment=\"Beta\"/>"
    "</Menu></MenuBar></CommandLayout></GlobalSettings></Scheme>";

// The export with the menu "Test" renamed, so it no longer fits the live menu bar
static std::string ExportForOtherMenuBar() {
    std::string xml(kExportXml);
    xml.replace(xml.find("&amp;Test"), 9, "Other");
    return xml;
}

static FILETIME TimeAt(int secondsFromBase) {
    ULARGE_INTEGER ticks;
    ticks.QuadPart = 133500000000000000ull + static_cast<unsigned long long>(1000 + secondsFromBase) * 10000000ull;
    return { ticks.LowPart, ticks.HighPart };
}

static void SetWriteTime(const std::filesystem::path& path, const FILETIME& time) {
    HANDLE file = CreateFileW(path.c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    assert(file != INVALID_HANDLE_VALUE);
    assert(SetFileTime(file, nullptr, nullptr, &time));
    CloseHandle(file);
}

static void WriteTestFile(const std::filesystem::path& path, const std::string& contents, int secondsFromBase) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << contents;
    SetWriteTime(path, TimeAt(secondsFromBase));
}

class TestFolders {
public:
    TestFolders() : m_root(std::filesystem::temp_directory_path() / (L"baaz_live_menu_" + std::to_wstring(GetCurrentProcessId()))) {
        std::filesystem::remove_all(m_root);
        std::filesystem::create_directories(Commands());
        std::filesystem::create_directories(AppData());
        CommandSource::SetFoldersForTest(Commands().wstring(), AppData().wstring());
    }
    ~TestFolders() { std::filesystem::remove_all(m_root); }
    TestFolders(const TestFolders&) = delete;
    TestFolders& operator=(const TestFolders&) = delete;

    std::filesystem::path Root() const { return m_root; }
    std::filesystem::path Commands() const { return m_root / L"commands"; }
    std::filesystem::path AppData() const { return m_root / L"appdata"; }
    void WriteExport(int secondsFromBase, const std::string& xml = kExportXml) const {
        WriteTestFile(Commands() / L"export.xml", xml, secondsFromBase);
    }
    // The scheme files of the running Archicad, two folders below its folder
    std::filesystem::path SchemePath(const wchar_t* archicadFolder = L"Archicad 29 TST R1") const {
        return AppData() / L"GRAPHISOFT" / archicadFolder / L"Workenv" / L"Commands" / L"layout.bin";
    }
    void WriteScheme(const std::string& contents, int secondsFromBase, const wchar_t* archicadFolder = L"Archicad 29 TST R1") const {
        WriteTestFile(SchemePath(archicadFolder), contents, secondsFromBase);
    }
    // An export that is newer than the scheme, so the export counts as current
    void WriteCurrent() const {
        WriteExport(60);
        WriteScheme(std::string(kSchemeMarker), 0);
    }

    void CreateArchicadFolder() const { std::filesystem::create_directories(AppData() / L"GRAPHISOFT" / L"Archicad 29 TST R1"); }

private:
    std::filesystem::path m_root;
};

// Holds a file open without sharing, like a program that is writing to it
class FileLock {
public:
    explicit FileLock(const std::filesystem::path& path)
        : m_file(CreateFileW(path.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)) {
        assert(m_file != INVALID_HANDLE_VALUE);
    }
    ~FileLock() { CloseHandle(m_file); }
    FileLock(const FileLock&) = delete;
    FileLock& operator=(const FileLock&) = delete;

private:
    HANDLE m_file;
};

static const std::wstring kOutOfDate = L"Command list is out of date. Export the command scheme again (see README).";

// --- Tests -----------------------------------------------------------------------------------

void TestFillsOnDemand() {
    TestFrame frame(Fill::Normal);
    assert(GetMenuItemCount(g_popup) == 0);
    LiveMenu live(frame.Hwnd());
    assert((live.TopLevelTitles() == std::vector<std::wstring>{ L"Test" }));
    std::vector<LiveEntry> entries = live.Entries({ 0 });
    assert(entries.size() == 3);
    assert(entries[0].type == MenuNodeType::Item && entries[0].liveId == 255 && entries[0].enabled);
    assert(entries[1].type == MenuNodeType::Separator);
    assert(entries[2].type == MenuNodeType::Item && entries[2].liveId == 300);
    assert(live.Entries({ 1 }).empty());
    assert(live.Entries({ 0, 0 }).empty());
    std::cout << "[PASS] TestFillsOnDemand\n";
}

void TestRunPostsCommand() {
    TestFrame frame(Fill::Normal);
    assert(CommandSource::Run(CommandItem(255, { 0, 0 }), frame.Hwnd()));
    Pump();
    assert((g_commands == std::vector<unsigned>{ 255 }));
    std::cout << "[PASS] TestRunPostsCommand\n";
}

void TestRunRefusesDisabled() {
    TestFrame frame(Fill::Grayed);
    assert(!CommandSource::Run(CommandItem(255, { 0, 0 }), frame.Hwnd()));
    Pump();
    assert(g_commands.empty());
    std::cout << "[PASS] TestRunRefusesDisabled\n";
}

void TestRunRefusesChangedId() {
    TestFrame frame(Fill::ChangedId);
    assert(!CommandSource::Run(CommandItem(255, { 0, 0 }), frame.Hwnd()));
    Pump();
    assert(g_commands.empty());
    std::cout << "[PASS] TestRunRefusesChangedId\n";
}

void TestRunRefusesWrongKindOrPosition() {
    TestFrame frame(Fill::Normal);
    SearchItem notACommand = CommandItem(255, { 0, 0 });
    notACommand.kind = ItemKind::Help;
    assert(!CommandSource::Run(notACommand, frame.Hwnd()));
    assert(!CommandSource::Run(CommandItem(255, { 0, 1 }), frame.Hwnd()));   // a separator
    assert(!CommandSource::Run(CommandItem(255, { 0, 7 }), frame.Hwnd()));   // beyond the menu
    assert(!CommandSource::Run(CommandItem(255, { 0 }), frame.Hwnd()));      // a menu, not an item
    assert(!CommandSource::Run(CommandItem(255, { 0, 0 }), nullptr));
    Pump();
    assert(g_commands.empty());
    std::cout << "[PASS] TestRunRefusesWrongKindOrPosition\n";
}

void TestStatusLineWhenStale() {
    TestFrame frame(Fill::Normal);
    TestFolders folders;
    folders.WriteExport(0);
    folders.WriteScheme(std::string("xx") + kSchemeMarker, 60);
    std::vector<SearchItem> items = CommandSource::BuildCommandIndex(frame.Hwnd(), {});
    assert(items.size() == 1);
    assert(items[0].kind == ItemKind::Help);
    assert(items[0].title == kOutOfDate);
    std::cout << "[PASS] TestStatusLineWhenStale\n";
}

void TestOffersCommandsWhenCurrent() {
    TestFrame frame(Fill::Normal);
    TestFolders folders;
    folders.WriteExport(60);
    folders.WriteScheme(std::string("xx") + kSchemeMarker, 0);
    std::vector<SearchItem> items = CommandSource::BuildCommandIndex(frame.Hwnd(), {});
    assert(items.size() == 2);
    assert(items[0].kind == ItemKind::Command && items[0].title == L"Alpha");
    assert(items[0].liveId == 255 && items[0].enabled && (items[0].menuPosition == std::vector<int>{ 0, 0 }));
    assert(items[1].title == L"Beta" && items[1].liveId == 300 && (items[1].menuPosition == std::vector<int>{ 0, 2 }));
    assert(CommandSource::Run(items[1], frame.Hwnd()));
    Pump();
    assert((g_commands == std::vector<unsigned>{ 300 }));
    std::cout << "[PASS] TestOffersCommandsWhenCurrent\n";
}

// Without a scheme file to compare with, the export cannot be called current: commands plus the note
static void AssertCommandsWithCannotCheckNote(HWND frame) {
    std::vector<SearchItem> items = CommandSource::BuildCommandIndex(frame, {});
    assert(items.size() == 3);
    assert(items[0].kind == ItemKind::Command && items[1].kind == ItemKind::Command);
    assert(items[2].kind == ItemKind::Help && items[2].title == L"Command list: cannot check whether the export is current");
}

void TestShiftedTopLevelPositionsAreFollowed() {
    TestFrame frame(Fill::Normal, true);   // live bar: Extra, Test, untitled slot; the export has Test only
    TestFolders folders;
    folders.WriteCurrent();
    std::vector<SearchItem> items = CommandSource::BuildCommandIndex(frame.Hwnd(), {});
    assert(items.size() == 2);
    assert(items[0].title == L"Alpha" && items[0].liveId == 255 && (items[0].menuPosition == std::vector<int>{ 1, 0 }));
    assert(items[1].title == L"Beta" && (items[1].menuPosition == std::vector<int>{ 1, 2 }));
    assert(CommandSource::Run(items[0], frame.Hwnd()));
    Pump();
    assert((g_commands == std::vector<unsigned>{ 255 }));
    frame.ChangeFill(Fill::ChangedId);
    assert(!CommandSource::Run(items[0], frame.Hwnd()));
    Pump();
    assert(g_commands.size() == 1);
    std::cout << "[PASS] TestShiftedTopLevelPositionsAreFollowed\n";
}

void TestSchemeFileWithoutMarkerIsUnknown() {
    TestFrame frame(Fill::Normal);
    TestFolders folders;
    folders.WriteExport(60);
    folders.WriteScheme("a newer file that is not a command layout", 120);
    AssertCommandsWithCannotCheckNote(frame.Hwnd());
    std::cout << "[PASS] TestSchemeFileWithoutMarkerIsUnknown\n";
}

void TestArchicadFolderWithoutSchemeFilesIsUnknown() {
    TestFrame frame(Fill::Normal);
    TestFolders folders;
    folders.WriteExport(60);
    folders.CreateArchicadFolder();
    AssertCommandsWithCannotCheckNote(frame.Hwnd());
    std::cout << "[PASS] TestArchicadFolderWithoutSchemeFilesIsUnknown\n";
}

void TestRunRefusesDisabledFrame() {
    TestFrame frame(Fill::Normal);
    EnableWindow(frame.Hwnd(), FALSE);   // a modal dialog is open on top of it
    assert(!CommandSource::Run(CommandItem(255, { 0, 0 }), frame.Hwnd()));
    Pump();
    assert(g_commands.empty());
    EnableWindow(frame.Hwnd(), TRUE);
    assert(CommandSource::Run(CommandItem(255, { 0, 0 }), frame.Hwnd()));
    Pump();
    assert((g_commands == std::vector<unsigned>{ 255 }));
    std::cout << "[PASS] TestRunRefusesDisabledFrame\n";
}

void TestRunAfterMenuRebuiltSinceIndexing() {
    TestFrame frame(Fill::Normal);
    TestFolders folders;
    folders.WriteCurrent();
    std::vector<SearchItem> items = CommandSource::BuildCommandIndex(frame.Hwnd(), {});
    assert(items.size() == 2);
    frame.ChangeFill(Fill::ChangedId);
    assert(!CommandSource::Run(items[0], frame.Hwnd()));
    frame.ChangeFill(Fill::Grayed);
    assert(!CommandSource::Run(items[0], frame.Hwnd()));
    frame.ChangeFill(Fill::Shifted);
    assert(!CommandSource::Run(items[0], frame.Hwnd()));
    assert(!CommandSource::Run(items[1], frame.Hwnd()));
    Pump();
    assert(g_commands.empty());
    frame.ChangeFill(Fill::Normal);
    assert(CommandSource::Run(items[0], frame.Hwnd()));
    Pump();
    assert((g_commands == std::vector<unsigned>{ 255 }));
    std::cout << "[PASS] TestRunAfterMenuRebuiltSinceIndexing\n";
}

void TestNestedPopups() {
    TestFrame frame(Fill::Nested);
    LiveMenu live(frame.Hwnd());
    std::vector<LiveEntry> top = live.Entries({ 0 });
    assert(top.size() == 4);
    assert(top[3].type == MenuNodeType::Popup && top[3].liveId == 0);
    std::vector<LiveEntry> sub = live.Entries({ 0, 3 });
    assert(sub.size() == 2 && sub[0].liveId == 500 && sub[1].liveId == 501);
    assert(CommandSource::Run(CommandItem(501, { 0, 3, 1 }), frame.Hwnd()));
    Pump();
    assert((g_commands == std::vector<unsigned>{ 501 }));
    assert(!CommandSource::Run(CommandItem(600, { 0, 3 }), frame.Hwnd()));   // the popup itself
    Pump();
    assert(g_commands.size() == 1);
    std::cout << "[PASS] TestNestedPopups\n";
}

void TestIndexOffersGreyedItemDisabled() {
    TestFrame frame(Fill::Grayed);
    TestFolders folders;
    folders.WriteCurrent();
    std::vector<SearchItem> items = CommandSource::BuildCommandIndex(frame.Hwnd(), {});
    assert(items.size() == 2);
    assert(items[0].kind == ItemKind::Command && items[0].title == L"Alpha" && !items[0].enabled);
    assert(items[1].enabled);
    assert(!CommandSource::Run(items[0], frame.Hwnd()));
    Pump();
    assert(g_commands.empty());
    std::cout << "[PASS] TestIndexOffersGreyedItemDisabled\n";
}

static std::vector<std::wstring> Titles(const std::vector<SearchItem>& items) {
    std::vector<std::wstring> titles;
    for (const SearchItem& item : items) titles.push_back(item.title);
    return titles;
}

// The Test menu of kExportXml plus a submenu "Sub" holding the given entries
static std::string ExportWithSub(const std::string& subEntries) {
    std::string xml(kExportXml);
    const std::string sub = subEntries.empty() ? "<PopupItem><Menu ID=\"S\" title=\"Sub\"/></PopupItem>"
                                               : "<PopupItem><Menu ID=\"S\" title=\"Sub\">" + subEntries + "</Menu></PopupItem>";
    xml.insert(xml.find("</Menu>"), sub);
    return xml;
}

static const std::vector<std::wstring> kAlphaBeta = { L"Alpha", L"Beta" };

static void AssertSingleOutOfDateLine(const std::vector<SearchItem>& items) {
    assert(items.size() == 1 && items[0].kind == ItemKind::Help && items[0].title == kOutOfDate);
}

void TestExportChoice() {
    {   // Neither a file without the schema nor an export of another menu bar gets in the way, whatever the names
        TestFrame frame(Fill::Normal);
        TestFolders folders;
        folders.WriteScheme(std::string(kSchemeMarker), 0);
        WriteTestFile(folders.Commands() / L"a_garbage.xml", "this is not a scheme", 60);
        WriteTestFile(folders.Commands() / L"b_other.xml", ExportForOtherMenuBar(), 60);
        WriteTestFile(folders.Commands() / L"c_good.xml", kExportXml, 60);
        assert(Titles(CommandSource::BuildCommandIndex(frame.Hwnd(), {})) == kAlphaBeta);
    }
    {
        TestFrame frame(Fill::Normal);
        TestFolders folders;
        folders.WriteScheme(std::string(kSchemeMarker), 0);
        WriteTestFile(folders.Commands() / L"a_good.xml", kExportXml, 60);
        WriteTestFile(folders.Commands() / L"b_other.xml", ExportForOtherMenuBar(), 60);
        WriteTestFile(folders.Commands() / L"c_garbage.xml", "this is not a scheme", 60);
        assert(Titles(CommandSource::BuildCommandIndex(frame.Hwnd(), {})) == kAlphaBeta);
    }
    {   // Two exports that fit equally well are lifted by a third one that links more
        TestFrame nested(Fill::Nested);
        TestFolders folders;
        folders.WriteScheme(std::string(kSchemeMarker), 0);
        WriteTestFile(folders.Commands() / L"a_tie.xml", ExportWithSub(""), 60);
        WriteTestFile(folders.Commands() / L"b_tie.xml", ExportWithSub(""), 60);
        assert(CommandSource::BuildCommandIndex(nested.Hwnd(), {}).size() == 1);   // the tie: one status line
        WriteTestFile(folders.Commands() / L"c_best.xml",
                      ExportWithSub("<Item command=\"3-0x0-0x0\" comment=\"S1\"/><Item command=\"4-0x0-0x0\" comment=\"S2\"/>"), 60);
        assert((Titles(CommandSource::BuildCommandIndex(nested.Hwnd(), {})) ==
                std::vector<std::wstring>{ L"Alpha", L"Beta", L"S1", L"S2" }));
    }
    {   // Several exports, none of which fits: the status line, not a tie result
        TestFrame frame(Fill::Normal);
        TestFolders folders;
        folders.WriteScheme(std::string(kSchemeMarker), 0);
        WriteTestFile(folders.Commands() / L"a_other.xml", ExportForOtherMenuBar(), 60);
        WriteTestFile(folders.Commands() / L"b_other.xml", ExportForOtherMenuBar(), 60);
        AssertSingleOutOfDateLine(CommandSource::BuildCommandIndex(frame.Hwnd(), {}));
    }
    std::cout << "[PASS] TestExportChoice\n";
}

void TestExtraEntryGivesStatusLine() {
    TestFrame frame(Fill::ExtraItem);
    TestFolders folders;
    folders.WriteCurrent();
    AssertSingleOutOfDateLine(CommandSource::BuildCommandIndex(frame.Hwnd(), {}));
    std::cout << "[PASS] TestExtraEntryGivesStatusLine\n";
}

void TestBuildCommandIndexWithoutMenu() {
    TestFolders folders;
    folders.WriteCurrent();
    assert(CommandSource::BuildCommandIndex(nullptr, {}).empty());
    HWND noMenu = CreateWindowExW(0, L"STATIC", L"no menu", WS_OVERLAPPED, 0, 0, 50, 50, nullptr, nullptr,
                                  GetModuleHandleW(nullptr), nullptr);
    assert(noMenu != nullptr);
    assert(CommandSource::BuildCommandIndex(noMenu, {}).empty());
    DestroyWindow(noMenu);
    std::cout << "[PASS] TestBuildCommandIndexWithoutMenu\n";
}

void TestSeveralArchicadFolders() {
    TestFrame frame(Fill::Normal);
    TestFolders folders;
    folders.WriteExport(60);
    folders.WriteScheme(std::string(kSchemeMarker), 0, L"Archicad 29 DEU R1");
    folders.WriteScheme(std::string(kSchemeMarker), 120, L"Archicad 29 INT R1");
    AssertSingleOutOfDateLine(CommandSource::BuildCommandIndex(frame.Hwnd(), {}));
    std::cout << "[PASS] TestSeveralArchicadFolders\n";
}

void TestOversizeExportIsNotOffered() {
    TestFrame frame(Fill::Normal);
    TestFolders folders;
    folders.WriteScheme(std::string(kSchemeMarker), 0);
    folders.WriteExport(60, std::string(kMaxInputBytes + 1, ' '));
    AssertSingleOutOfDateLine(CommandSource::BuildCommandIndex(frame.Hwnd(), {}));
    folders.WriteExport(61);
    assert(CommandSource::BuildCommandIndex(frame.Hwnd(), {}).size() == 2);
    std::cout << "[PASS] TestOversizeExportIsNotOffered\n";
}

void TestUnreadableExportIsRetried() {
    TestFrame frame(Fill::Normal);
    TestFolders folders;
    folders.WriteCurrent();
    {
        FileLock lock(folders.Commands() / L"export.xml");
        AssertSingleOutOfDateLine(CommandSource::BuildCommandIndex(frame.Hwnd(), {}));
    }
    assert(CommandSource::BuildCommandIndex(frame.Hwnd(), {}).size() == 2);   // same file time, read again
    std::cout << "[PASS] TestUnreadableExportIsRetried\n";
}

void TestDirectoryNamedLikeAnExportIsSkipped() {
    TestFrame frame(Fill::Normal);
    TestFolders folders;
    folders.WriteCurrent();
    std::filesystem::create_directories(folders.Commands() / L"folder.xml");
    assert(CommandSource::BuildCommandIndex(frame.Hwnd(), {}).size() == 2);
    std::cout << "[PASS] TestDirectoryNamedLikeAnExportIsSkipped\n";
}

void TestLockedSchemeFileIsUnknown() {
    TestFrame frame(Fill::Normal);
    TestFolders folders;
    folders.WriteCurrent();
    WriteTestFile(folders.SchemePath().parent_path() / L"other.bin", kSchemeMarker, 0);
    FileLock lock(folders.SchemePath());
    AssertCommandsWithCannotCheckNote(frame.Hwnd());
    std::cout << "[PASS] TestLockedSchemeFileIsUnknown\n";
}

// Without a user profile the commands folder is unknown; a ".baaz\commands" below the current folder is not it
void TestNoUserProfileOffersNothing() {
    TestFrame frame(Fill::Normal);
    TestFolders folders;
    WriteTestFile(folders.Root() / L".baaz" / L"commands" / L"export.xml", kExportXml, 60);
    wchar_t savedProfile[2048] = {};
    const DWORD savedLength = GetEnvironmentVariableW(L"USERPROFILE", savedProfile, 2048);
    wchar_t savedDirectory[2048] = {};
    assert(GetCurrentDirectoryW(2048, savedDirectory) > 0);
    assert(SetCurrentDirectoryW(folders.Root().c_str()));
    assert(SetEnvironmentVariableW(L"USERPROFILE", nullptr));
    CommandSource::SetFoldersForTest(L"", folders.AppData().wstring());   // no test override: the real lookup
    const std::vector<SearchItem> items = CommandSource::BuildCommandIndex(frame.Hwnd(), {});
    SetCurrentDirectoryW(savedDirectory);
    if (savedLength > 0) SetEnvironmentVariableW(L"USERPROFILE", savedProfile);
    assert(items.empty());
    std::cout << "[PASS] TestNoUserProfileOffersNothing\n";
}

void TestNoExportOffersNothing() {
    TestFrame frame(Fill::Normal);
    TestFolders folders;
    assert(CommandSource::BuildCommandIndex(frame.Hwnd(), {}).empty());
    std::cout << "[PASS] TestNoExportOffersNothing\n";
}

void TestExportForOtherMenuBarIsOutOfDate() {
    TestFrame frame(Fill::Normal);
    TestFolders folders;
    folders.WriteExport(60, ExportForOtherMenuBar());
    folders.WriteScheme(std::string(kSchemeMarker), 0);
    std::vector<SearchItem> items = CommandSource::BuildCommandIndex(frame.Hwnd(), {});
    assert(items.size() == 1 && items[0].kind == ItemKind::Help && items[0].title == kOutOfDate);
    std::cout << "[PASS] TestExportForOtherMenuBarIsOutOfDate\n";
}

void TestTwoEquallyFittingExportsOfferNothing() {
    TestFrame frame(Fill::Normal);
    TestFolders folders;
    folders.WriteExport(60);
    WriteTestFile(folders.Commands() / L"copy.xml", kExportXml, 60);
    folders.WriteScheme(std::string(kSchemeMarker), 0);
    std::vector<SearchItem> items = CommandSource::BuildCommandIndex(frame.Hwnd(), {});
    assert(items.size() == 1 && items[0].kind == ItemKind::Help && items[0].title == kOutOfDate);
    std::cout << "[PASS] TestTwoEquallyFittingExportsOfferNothing\n";
}

void TestUncheckableIsNotedOnce() {
    TestFrame frame(Fill::Normal);
    TestFolders folders;
    folders.WriteExport(60);   // no Archicad folder under appdata, so staleness cannot be checked
    std::vector<SearchItem> first = CommandSource::BuildCommandIndex(frame.Hwnd(), {});
    assert(first.size() == 3);
    assert(first[0].kind == ItemKind::Command && first[1].kind == ItemKind::Command);
    assert(first[2].kind == ItemKind::Help);
    assert(first[2].title == L"Command list: cannot check whether the export is current");
    assert(CommandSource::BuildCommandIndex(frame.Hwnd(), {}).size() == 2);
    std::cout << "[PASS] TestUncheckableIsNotedOnce\n";
}

void TestExportIsReReadOnlyWhenItsTimeChanges() {
    TestFrame frame(Fill::Normal);
    TestFolders folders;
    folders.WriteExport(60);
    folders.WriteScheme(std::string(kSchemeMarker), 0);
    assert(CommandSource::BuildCommandIndex(frame.Hwnd(), {}).size() == 2);
    folders.WriteExport(60, ExportForOtherMenuBar());   // same file time: the first parse is kept
    assert(CommandSource::BuildCommandIndex(frame.Hwnd(), {}).size() == 2);
    folders.WriteExport(61, ExportForOtherMenuBar());   // new file time: read again, and it no longer fits
    std::vector<SearchItem> items = CommandSource::BuildCommandIndex(frame.Hwnd(), {});
    assert(items.size() == 1 && items[0].kind == ItemKind::Help);
    std::cout << "[PASS] TestExportIsReReadOnlyWhenItsTimeChanges\n";
}

int main() {
    TestFillsOnDemand();
    TestRunPostsCommand();
    TestRunRefusesDisabled();
    TestRunRefusesChangedId();
    TestRunRefusesWrongKindOrPosition();
    TestRunRefusesDisabledFrame();
    TestRunAfterMenuRebuiltSinceIndexing();
    TestNestedPopups();
    TestIndexOffersGreyedItemDisabled();
    TestStatusLineWhenStale();
    TestOffersCommandsWhenCurrent();
    TestShiftedTopLevelPositionsAreFollowed();
    TestSchemeFileWithoutMarkerIsUnknown();
    TestArchicadFolderWithoutSchemeFilesIsUnknown();
    TestExportChoice();
    TestExtraEntryGivesStatusLine();
    TestBuildCommandIndexWithoutMenu();
    TestSeveralArchicadFolders();
    TestOversizeExportIsNotOffered();
    TestUnreadableExportIsRetried();
    TestDirectoryNamedLikeAnExportIsSkipped();
    TestLockedSchemeFileIsUnknown();
    TestNoUserProfileOffersNothing();
    TestNoExportOffersNothing();
    TestExportForOtherMenuBarIsOutOfDate();
    TestTwoEquallyFittingExportsOfferNothing();
    TestUncheckableIsNotedOnce();
    TestExportIsReReadOnlyWhenItsTimeChanges();
    std::cout << "All live menu tests passed.\n";
    return 0;
}
