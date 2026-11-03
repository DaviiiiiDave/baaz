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

enum class Fill { Normal, Grayed, ChangedId };

static Fill g_fill = Fill::Normal;
static HMENU g_popup = nullptr;
static std::vector<unsigned> g_commands;

static void FillPopup(HMENU popup) {
    while (GetMenuItemCount(popup) > 0) DeleteMenu(popup, 0, MF_BYPOSITION);
    UINT firstFlags = g_fill == Fill::Grayed ? (MF_STRING | MF_GRAYED) : MF_STRING;
    UINT firstId = g_fill == Fill::ChangedId ? 256 : 255;
    AppendMenuW(popup, firstFlags, firstId, L"A&lpha\tCtrl+A");
    AppendMenuW(popup, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(popup, MF_STRING, 300, L"Beta");
}

static LRESULT CALLBACK FrameProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_INITMENUPOPUP && reinterpret_cast<HMENU>(wParam) == g_popup) {
        FillPopup(g_popup);
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
    explicit TestFrame(Fill fill) {
        g_fill = fill;
        g_commands.clear();
        static const wchar_t* const kClass = L"BaazLiveMenuTestFrame";
        WNDCLASSW windowClass = {};
        windowClass.lpfnWndProc = FrameProc;
        windowClass.hInstance = GetModuleHandleW(nullptr);
        windowClass.lpszClassName = kClass;
        RegisterClassW(&windowClass);
        HMENU bar = CreateMenu();
        g_popup = CreatePopupMenu();
        AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(g_popup), L"&Test");
        m_hwnd = CreateWindowExW(0, kClass, L"frame", WS_OVERLAPPEDWINDOW, 0, 0, 200, 100, nullptr, bar,
                                 windowClass.hInstance, nullptr);
        assert(m_hwnd != nullptr);
    }
    ~TestFrame() {
        DestroyWindow(m_hwnd);
        g_popup = nullptr;
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

    std::filesystem::path Commands() const { return m_root / L"commands"; }
    std::filesystem::path AppData() const { return m_root / L"appdata"; }
    void WriteExport(int secondsFromBase, const std::string& xml = kExportXml) const {
        WriteTestFile(Commands() / L"export.xml", xml, secondsFromBase);
    }
    // The scheme files of the running Archicad, three folders below its folder
    void WriteScheme(const std::string& contents, int secondsFromBase) const {
        WriteTestFile(AppData() / L"GRAPHISOFT" / L"Archicad 29 TST R1" / L"Workenv" / L"Commands" / L"layout.bin",
                  contents, secondsFromBase);
    }

    void CreateArchicadFolder() const { std::filesystem::create_directories(AppData() / L"GRAPHISOFT" / L"Archicad 29 TST R1"); }

private:
    std::filesystem::path m_root;
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
    TestStatusLineWhenStale();
    TestOffersCommandsWhenCurrent();
    TestSchemeFileWithoutMarkerIsUnknown();
    TestArchicadFolderWithoutSchemeFilesIsUnknown();
    TestNoExportOffersNothing();
    TestExportForOtherMenuBarIsOutOfDate();
    TestTwoEquallyFittingExportsOfferNothing();
    TestUncheckableIsNotedOnce();
    TestExportIsReReadOnlyWhenItsTimeChanges();
    std::cout << "All live menu tests passed.\n";
    return 0;
}
