#pragma once

#include <windows.h>

namespace Baaz {

// Defined in BaazAddon.cpp; the hook test provides its own.
HINSTANCE AddonInstance();
HWND MainWindow();

} // namespace Baaz
