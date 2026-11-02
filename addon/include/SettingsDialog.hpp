#pragma once

#include <windows.h>
#include <vector>

namespace Baaz {

// Native Win32 Settings / Help / About window
class SettingsDialog {
public:
    static void Show(HWND hParent, int initialTab = 0);
    static bool IsOpen();
    static void Close();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static void CreateControls(HWND hwnd);
    static void SwitchTab(int tabIndex);
    static void LoadValuesToUI();
    static void SaveValuesFromUI();

    static HWND s_hwnd;
    static HWND s_hTabControl;
    static HWND s_hHoldMsEdit;
    static HWND s_hMaxResultsEdit;
    static HWND s_hEnableChordsCheck;
    static HWND s_hSaveBtn;
    static HWND s_hHelpText;
    static HWND s_hAboutText;
    static std::vector<HWND> s_settingsControls;
};

} // namespace Baaz
