#pragma once

#include <windows.h>

namespace Baaz {

// Marks the Space key events and the click (press and release) Baaz synthesizes for Magic Wand. The
// hook lets the key events through on this mark; the replayed press passes because the hold is
// already confirmed, the replayed release because no replay is waiting for it any more.
static const ULONG_PTR SENTINEL_MAGIC_WAND = 0x53504143;

// Thread hooks on Archicad's UI thread: Space tap opens the HUD, Space hold
// (or Space + click) becomes Archicad's own Magic Wand, and while the HUD is
// open every keystroke goes to it.
class MessageHook {
public:
    static bool Install(HWND hMainWnd);
    static void Uninstall();

private:
    static LRESULT CALLBACK GetMsgHookProc(int code, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK MouseHookProc(int code, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK CallWndHookProc(int code, WPARAM wParam, LPARAM lParam);
    static void CALLBACK HoldTimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);

    static bool CanSummonHud();
    static void CancelHoldTimer();
    static void ResetHold();

    static HHOOK s_hGetMsgHook;
    static HHOOK s_hMouseHook;
    static HHOOK s_hCallWndHook;
    static HWND s_hMainWnd;
    static UINT_PTR s_holdTimerId;
    static bool s_spaceDown;
    static bool s_holdConfirmed;
    static bool s_swallowNextLButtonUp;
    static bool s_swallowTrailingKeyUp;
    static UINT s_replayedButtonUp;  // WM_LBUTTONUP or WM_RBUTTONUP while a replayed click awaits its release, else 0
};

} // namespace Baaz
