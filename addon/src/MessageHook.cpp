#include "MessageHook.hpp"
#include "HudWindow.hpp"
#include "NavigatorIndexer.hpp"
#include "Settings.hpp"
#include <cwctype>

namespace Baaz {

HHOOK MessageHook::s_hGetMsgHook = NULL;
HHOOK MessageHook::s_hMouseHook = NULL;
HHOOK MessageHook::s_hCallWndHook = NULL;
HWND MessageHook::s_hMainWnd = NULL;
UINT_PTR MessageHook::s_holdTimerId = 0;
bool MessageHook::s_spaceDown = false;
bool MessageHook::s_holdConfirmed = false;
bool MessageHook::s_swallowNextLButtonUp = false;
bool MessageHook::s_swallowTrailingKeyUp = false;
UINT MessageHook::s_replayedButtonUp = 0;

static bool IsKeyMessage(UINT msg) {
    return msg == WM_KEYDOWN || msg == WM_KEYUP || msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP;
}

static bool IsKeyUpMessage(UINT msg) {
    return msg == WM_KEYUP || msg == WM_SYSKEYUP;
}

static void SendWandSpace(bool keyUp) {
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = VK_SPACE;
    input.ki.dwFlags = keyUp ? KEYEVENTF_KEYUP : 0;
    input.ki.dwExtraInfo = SENTINEL_MAGIC_WAND;
    SendInput(1, &input, sizeof(INPUT));
}

// False if the system refused the event.
static bool SendWandMouse(DWORD flags) {
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = flags;
    input.mi.dwExtraInfo = SENTINEL_MAGIC_WAND;
    return SendInput(1, &input, sizeof(INPUT)) == 1;
}

// Archicad 29 takes Space from its own key message, so the click Space modifies
// has to arrive after it; the real click is withheld and replayed behind Space.
static bool SendWandClick(UINT downMsg) {
    return SendWandMouse((downMsg == WM_LBUTTONDOWN) ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_RIGHTDOWN);
}

static bool SendWandRelease(UINT upMsg) {
    return SendWandMouse((upMsg == WM_LBUTTONUP) ? MOUSEEVENTF_LEFTUP : MOUSEEVENTF_RIGHTUP);
}

static void Unhook(HHOOK& hook) {
    if (hook != NULL) {
        UnhookWindowsHookEx(hook);
        hook = NULL;
    }
}

// Typing into an editable field must keep its spaces
static bool IsTextEditingActive(HWND hFocused) {
    wchar_t cls[64] = {0};
    if (!hFocused || GetClassNameW(hFocused, cls, 63) == 0) return false;

    bool isEditClass = (_wcsicmp(cls, L"Edit") == 0 ||
                        _wcsnicmp(cls, L"RichEdit", 8) == 0 ||
                        _wcsicmp(cls, L"DGUniRichEditClass") == 0 ||
                        _wcsicmp(cls, L"Scintilla") == 0);
    return isEditClass && !(GetWindowLongPtrW(hFocused, GWL_STYLE) & ES_READONLY);
}

bool MessageHook::Install(HWND hMainWnd) {
    s_hMainWnd = hMainWnd;
    DWORD threadId = GetCurrentThreadId();
    s_hGetMsgHook = SetWindowsHookExW(WH_GETMESSAGE, GetMsgHookProc, NULL, threadId);
    s_hMouseHook = SetWindowsHookExW(WH_MOUSE, MouseHookProc, NULL, threadId);
    s_hCallWndHook = SetWindowsHookExW(WH_CALLWNDPROC, CallWndHookProc, NULL, threadId);
    return (s_hGetMsgHook != NULL && s_hMouseHook != NULL && s_hCallWndHook != NULL);
}

void MessageHook::Uninstall() {
    ResetHold();
    Unhook(s_hGetMsgHook);
    Unhook(s_hMouseHook);
    Unhook(s_hCallWndHook);
}

bool MessageHook::CanSummonHud() {
    // A disabled main window means a modal dialog is open
    return !IsTextEditingActive(GetFocus()) &&
           !(s_hMainWnd && !IsWindowEnabled(s_hMainWnd)) &&
           NavigatorIndexer::HasOpenProject();
}

void MessageHook::CancelHoldTimer() {
    if (s_holdTimerId != 0) {
        KillTimer(NULL, s_holdTimerId);
        s_holdTimerId = 0;
    }
}

void MessageHook::ResetHold() {
    CancelHoldTimer();
    s_spaceDown = false;
    s_holdConfirmed = false;
}

void CALLBACK MessageHook::HoldTimerProc(HWND /*hwnd*/, UINT /*uMsg*/, UINT_PTR idEvent, DWORD /*dwTime*/) {
    // SetTimer timers repeat, so always kill the one that fired, even if it is
    // a stale id: a leftover timer would turn the next Space tap into a hold.
    KillTimer(NULL, idEvent);
    if (idEvent != s_holdTimerId) return;
    s_holdTimerId = 0;

    if (s_spaceDown && !s_holdConfirmed) {
        s_holdConfirmed = true;
        SendWandSpace(false);
    }
}

LRESULT CALLBACK MessageHook::GetMsgHookProc(int code, WPARAM wParam, LPARAM lParam) {
    MSG* pMsg = reinterpret_cast<MSG*>(lParam);
    const bool relevant = pMsg && (IsKeyMessage(pMsg->message) ||
                                   pMsg->message == WM_LBUTTONDOWN || pMsg->message == WM_RBUTTONDOWN ||
                                   pMsg->message == WM_LBUTTONUP || pMsg->message == WM_RBUTTONUP);
    if (code < 0 || !relevant) {
        return CallNextHookEx(s_hGetMsgHook, code, wParam, lParam);
    }

    auto swallow = [pMsg]() -> LRESULT {
        pMsg->message = WM_NULL;
        return 0;
    };

    const UINT msg = pMsg->message;
    const bool isKey = IsKeyMessage(msg);
    const bool isSpace = isKey && pMsg->wParam == VK_SPACE;

    // Our own Magic Wand Space events go straight to Archicad
    if (isKey && GetMessageExtraInfo() == static_cast<LPARAM>(SENTINEL_MAGIC_WAND)) {
        return CallNextHookEx(s_hGetMsgHook, code, wParam, lParam);
    }

    // PeekMessage(PM_NOREMOVE) runs this hook too, and the same message comes
    // back when it is actually removed. Only the removing pass may change state;
    // a peek just sees the message hidden the same way the real pass will hide it.
    if (wParam == PM_NOREMOVE) {
        if (isKey && (HudWindow::IsVisible() || (IsKeyUpMessage(msg) && s_swallowTrailingKeyUp) ||
                      (isSpace && (s_spaceDown || (msg == WM_KEYDOWN && CanSummonHud()))))) {
            return swallow();
        }
        return CallNextHookEx(s_hGetMsgHook, code, wParam, lParam);
    }

    // Keyup of the Enter/Space that ran a command from the HUD
    if (IsKeyUpMessage(msg) && s_swallowTrailingKeyUp) {
        s_swallowTrailingKeyUp = false;
        return swallow();
    }

    // 1. HUD open: it takes every key it understands
    if (HudWindow::IsVisible() && isKey) {
        if (IsKeyUpMessage(msg)) return swallow();

        const WPARAM vk = pMsg->wParam;
        WCHAR ch = 0;
        bool handled = (vk == VK_ESCAPE || vk == VK_RETURN || vk == VK_SPACE ||
                        vk == VK_UP || vk == VK_DOWN || vk == VK_TAB || vk == VK_BACK);
        if (!handled) {
            BYTE keyboardState[256];
            GetKeyboardState(keyboardState);
            WCHAR buffer[4] = {0};
            int res = ToUnicode(static_cast<UINT>(vk), HIWORD(pMsg->lParam) & 0xFF, keyboardState, buffer, 4, 0);
            handled = (res > 0 && iswprint(buffer[0]));
            ch = buffer[0];
        }
        if (!handled) return CallNextHookEx(s_hGetMsgHook, code, wParam, lParam);

        // Enter and Space may run a command and close the HUD; keep their keyup from Archicad
        if (vk == VK_RETURN || vk == VK_SPACE) s_swallowTrailingKeyUp = true;
        HudWindow::HandleKey(vk, ch);
        return swallow();
    }

    // 2. HUD closed: Space tap vs hold
    if (msg == WM_KEYDOWN && isSpace) {
        if (pMsg->lParam & (1 << 30)) return swallow(); // typematic repeat
        if (!CanSummonHud()) return CallNextHookEx(s_hGetMsgHook, code, wParam, lParam);

        s_spaceDown = true;
        s_holdConfirmed = false;
        CancelHoldTimer();
        s_holdTimerId = SetTimer(NULL, 0, Settings::Get().spacebarHoldMs, HoldTimerProc);
        return swallow();
    }

    if (msg == WM_KEYUP && isSpace && s_spaceDown) {
        bool wasHold = s_holdConfirmed;
        ResetHold();
        if (wasHold) {
            SendWandSpace(true);
        } else {
            HudWindow::Show(s_hMainWnd);
        }
        return swallow();
    }

    // A click while Space is down engages Magic Wand right away
    if ((msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN) && s_spaceDown && !s_holdConfirmed) {
        CancelHoldTimer();
        s_holdConfirmed = true;

        // Archicad reads the key state when it handles this click
        BYTE keys[256];
        GetKeyboardState(keys);
        keys[VK_SPACE] |= 0x80;
        SetKeyboardState(keys);
        SendWandSpace(false);
        // Refused: the user's own click still reaches Archicad rather than vanishing
        if (!SendWandClick(msg)) return CallNextHookEx(s_hGetMsgHook, code, wParam, lParam);
        s_replayedButtonUp = (msg == WM_LBUTTONDOWN) ? WM_LBUTTONUP : WM_RBUTTONUP;
        return swallow();
    }

    // The release of a replayed click has to follow the replayed press, however early the
    // user lets go; the real one is withheld and replayed. The replayed one passes, because
    // the marker is cleared. Nothing is waiting while s_replayedButtonUp is 0.
    if (msg == s_replayedButtonUp) {
        s_replayedButtonUp = 0;
        if (SendWandRelease(msg)) return swallow();
    }

    return CallNextHookEx(s_hGetMsgHook, code, wParam, lParam);
}

// WM_ACTIVATEAPP is sent, not posted, so only a WH_CALLWNDPROC hook sees it.
// Switching apps closes the HUD and abandons a pending Space hold, so the hold
// timer can't inject a Magic Wand Space into the newly active app.
LRESULT CALLBACK MessageHook::CallWndHookProc(int code, WPARAM wParam, LPARAM lParam) {
    const CWPSTRUCT* pCwp = reinterpret_cast<const CWPSTRUCT*>(lParam);
    if (code >= 0 && pCwp && pCwp->message == WM_ACTIVATEAPP && pCwp->wParam == FALSE) {
        ResetHold();
        if (HudWindow::IsVisible()) HudWindow::Hide();
    }
    return CallNextHookEx(s_hCallWndHook, code, wParam, lParam);
}

LRESULT CALLBACK MessageHook::MouseHookProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code < 0) {
        return CallNextHookEx(s_hMouseHook, code, wParam, lParam);
    }

    if (wParam == WM_LBUTTONUP && s_swallowNextLButtonUp) {
        s_swallowNextLButtonUp = false;
        return 1;
    }

    const MOUSEHOOKSTRUCT* pmhs = reinterpret_cast<const MOUSEHOOKSTRUCT*>(lParam);
    if (HudWindow::IsVisible() && pmhs && (wParam == WM_LBUTTONDOWN || wParam == WM_RBUTTONDOWN)) {
        // Either way the button-up must not reach the canvas: a click on a
        // result closes the HUD before the button is released.
        if (wParam == WM_LBUTTONDOWN) s_swallowNextLButtonUp = true;

        RECT rc = HudWindow::GetBoundingRectScreen();
        if (PtInRect(&rc, pmhs->pt)) {
            return CallNextHookEx(s_hMouseHook, code, wParam, lParam);
        }
        HudWindow::Hide();
        return 1;
    }

    return CallNextHookEx(s_hMouseHook, code, wParam, lParam);
}

} // namespace Baaz
