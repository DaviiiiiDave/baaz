#include "SettingsDialog.hpp"
#include "Settings.hpp"
#include "SearchEngine.hpp"
#include "BaazAddon.hpp"
#include <commctrl.h>
#include <string>

namespace Baaz {

HWND SettingsDialog::s_hwnd = NULL;
HWND SettingsDialog::s_hTabControl = NULL;
HWND SettingsDialog::s_hHoldMsEdit = NULL;
HWND SettingsDialog::s_hMaxResultsEdit = NULL;
HWND SettingsDialog::s_hEnableChordsCheck = NULL;
HWND SettingsDialog::s_hSaveBtn = NULL;
HWND SettingsDialog::s_hHelpText = NULL;
HWND SettingsDialog::s_hAboutText = NULL;
std::vector<HWND> SettingsDialog::s_settingsControls;

static const wchar_t* DIALOG_CLASS = L"BaazSettingsDialogClass";
static const int ID_TABS = 1001;

bool SettingsDialog::IsOpen() {
    return (s_hwnd != NULL && IsWindow(s_hwnd));
}

void SettingsDialog::Close() {
    if (IsOpen()) {
        DestroyWindow(s_hwnd);
        s_hwnd = NULL;
    }
}

void SettingsDialog::SwitchTab(int tabIndex) {
    TabCtrl_SetCurSel(s_hTabControl, tabIndex);
    for (HWND control : s_settingsControls) {
        ShowWindow(control, tabIndex == 0 ? SW_SHOW : SW_HIDE);
    }
    ShowWindow(s_hHelpText, tabIndex == 1 ? SW_SHOW : SW_HIDE);
    ShowWindow(s_hAboutText, tabIndex == 2 ? SW_SHOW : SW_HIDE);
}

void SettingsDialog::Show(HWND hParent, int initialTab) {
    if (IsOpen()) {
        SwitchTab(initialTab);
        SetForegroundWindow(s_hwnd);
        return;
    }

    HINSTANCE hInst = AddonInstance();

    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = DIALOG_CLASS;
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassExW(&wc);

    INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX), ICC_TAB_CLASSES | ICC_UPDOWN_CLASS };
    InitCommonControlsEx(&icex);

    const int dlgWidth = 540;
    const int dlgHeight = 440;
    int posX = 100, posY = 100;
    if (hParent && IsWindow(hParent)) {
        RECT parentRc;
        GetWindowRect(hParent, &parentRc);
        posX = parentRc.left + (parentRc.right - parentRc.left - dlgWidth) / 2;
        posY = parentRc.top + (parentRc.bottom - parentRc.top - dlgHeight) / 2;
    }

    s_hwnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        DIALOG_CLASS, L"Baaz - Settings & Help",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        posX, posY, dlgWidth, dlgHeight,
        hParent, NULL, hInst, NULL
    );

    if (s_hwnd) {
        SwitchTab(initialTab);
        UpdateWindow(s_hwnd);
    }
}

void SettingsDialog::CreateControls(HWND hwnd) {
    HINSTANCE hInst = AddonInstance();
    HFONT hFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

    auto makeChild = [&](DWORD exStyle, const wchar_t* cls, const wchar_t* text, DWORD style,
                         int x, int y, int w, int h, int id = 0) {
        HWND child = CreateWindowExW(exStyle, cls, text, WS_CHILD | style, x, y, w, h,
                                     hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), hInst, NULL);
        SendMessage(child, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
        return child;
    };
    auto makeSpin = [&](HWND buddy, uint32_t min, uint32_t max) {
        HWND spin = makeChild(0, UPDOWN_CLASSW, L"", UDS_SETBUDDYINT | UDS_ALIGNRIGHT | UDS_ARROWKEYS | UDS_NOTHOUSANDS, 0, 0, 0, 0);
        SendMessage(spin, UDM_SETBUDDY, reinterpret_cast<WPARAM>(buddy), 0);
        SendMessage(spin, UDM_SETRANGE32, min, max);
        return spin;
    };

    s_hTabControl = makeChild(0, WC_TABCONTROLW, L"", WS_CLIPSIBLINGS | WS_VISIBLE, 15, 15, 495, 330, ID_TABS);
    TCITEMW tie = { TCIF_TEXT };
    for (const wchar_t* label : { L"Settings", L"Help & Shortcuts", L"About" }) {
        tie.pszText = const_cast<LPWSTR>(label);
        TabCtrl_InsertItem(s_hTabControl, TabCtrl_GetItemCount(s_hTabControl), &tie);
    }

    // Tab 0: settings
    s_hHoldMsEdit = makeChild(WS_EX_CLIENTEDGE, L"EDIT", L"", ES_NUMBER | ES_RIGHT, 320, 58, 60, 24);
    s_hMaxResultsEdit = makeChild(WS_EX_CLIENTEDGE, L"EDIT", L"", ES_NUMBER | ES_RIGHT, 320, 98, 60, 24);
    s_hEnableChordsCheck = makeChild(0, L"BUTTON", L"Enable Two-Letter Tool Chords (WA, DR, WN, SL, etc.)",
                                     BS_AUTOCHECKBOX, 35, 145, 420, 24);
    s_settingsControls = {
        makeChild(0, L"STATIC", L"Spacebar Hold Threshold (Magic Wand):", 0, 35, 60, 280, 20),
        s_hHoldMsEdit,
        makeSpin(s_hHoldMsEdit, kHoldMsMin, kHoldMsMax),
        makeChild(0, L"STATIC", L"ms", 0, 385, 60, 40, 20),
        makeChild(0, L"STATIC", L"Maximum Search Results Shown:", 0, 35, 100, 280, 20),
        s_hMaxResultsEdit,
        makeSpin(s_hMaxResultsEdit, kMaxResultsMin, kMaxResultsMax),
        s_hEnableChordsCheck,
    };

    // Tab 1: help, generated from the same tool table as the HUD's "?" results
    s_hHelpText = makeChild(WS_EX_CLIENTEDGE, L"EDIT", L"", ES_MULTILINE | ES_READONLY | WS_VSCROLL | ES_AUTOVSCROLL,
                            35, 55, 455, 270);
    std::wstring helpText = L"BAAZ KEYBOARD SHORTCUTS\r\n\r\n";
    for (const auto& line : SearchEngine::HelpLines(3)) {
        helpText += line + L"\r\n";
    }
    SetWindowTextW(s_hHelpText, helpText.c_str());

    // Tab 2: about
    s_hAboutText = makeChild(WS_EX_CLIENTEDGE, L"EDIT", L"", ES_MULTILINE | ES_READONLY | WS_VSCROLL, 35, 55, 455, 270);
    SetWindowTextW(s_hAboutText,
        L"Baaz for Archicad\r\n"
        L"Version 1.0.0\r\n\r\n"
        L"Keyboard-driven command palette: tap Space to search tools, views,\r\n"
        L"sheets and layer combinations; hold Space for Magic Wand.\r\n\r\n"
        L"Settings file: %USERPROFILE%\\.baaz\\config.json\r\n");

    s_hSaveBtn = makeChild(0, L"BUTTON", L"Save Settings", WS_VISIBLE | BS_DEFPUSHBUTTON, 285, 360, 110, 28, IDOK);
    s_settingsControls.push_back(s_hSaveBtn);
    makeChild(0, L"BUTTON", L"Close", WS_VISIBLE | BS_PUSHBUTTON, 405, 360, 100, 28, IDCANCEL);

    LoadValuesToUI();
}

void SettingsDialog::LoadValuesToUI() {
    const SettingsConfig& cfg = Settings::Get();
    SetWindowTextW(s_hHoldMsEdit, std::to_wstring(cfg.spacebarHoldMs).c_str());
    SetWindowTextW(s_hMaxResultsEdit, std::to_wstring(cfg.maxResults).c_str());
    SendMessage(s_hEnableChordsCheck, BM_SETCHECK, cfg.enableChords ? BST_CHECKED : BST_UNCHECKED, 0);
}

void SettingsDialog::SaveValuesFromUI() {
    // Settings::Set keeps the current value for anything out of range
    auto readNumber = [](HWND edit) {
        wchar_t buf[32] = {0};
        GetWindowTextW(edit, buf, 31);
        return static_cast<uint32_t>(wcstoul(buf, nullptr, 10));
    };

    SettingsConfig cfg = Settings::Get();
    cfg.spacebarHoldMs = readNumber(s_hHoldMsEdit);
    cfg.maxResults = readNumber(s_hMaxResultsEdit);
    cfg.enableChords = (SendMessage(s_hEnableChordsCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
    Settings::Set(cfg);
}

LRESULT CALLBACK SettingsDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        CreateControls(hwnd);
        return 0;

    case WM_NOTIFY: {
        LPNMHDR pnm = reinterpret_cast<LPNMHDR>(lParam);
        if (pnm->idFrom == ID_TABS && pnm->code == TCN_SELCHANGE) {
            SwitchTab(TabCtrl_GetCurSel(s_hTabControl));
        }
        break;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK) {
            SaveValuesFromUI();
            Close();
            return 0;
        }
        if (LOWORD(wParam) == IDCANCEL) {
            Close();
            return 0;
        }
        break;

    case WM_CLOSE:
        Close();
        return 0;

    case WM_DESTROY:
        s_hwnd = NULL;
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace Baaz
