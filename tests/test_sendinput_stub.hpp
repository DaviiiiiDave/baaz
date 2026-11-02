#pragma once

// Force-included into the hook sources by test_message_hook so synthesized
// keystrokes are recorded instead of being typed into whatever window has focus.
#include <windows.h>

UINT WINAPI TestSendInput(UINT cInputs, LPINPUT pInputs, int cbSize);
#define SendInput TestSendInput
