#pragma once

#include <string>
#include <vector>

namespace Baaz {

// Larger exports are rejected
constexpr size_t kMaxInputBytes = 8 * 1024 * 1024;

enum class MenuNodeType { Item, Separator, Popup };

struct MenuNode {
    MenuNodeType type = MenuNodeType::Item;
    std::wstring name;               // comment or title; access keys removed
    unsigned commandNumber = 0;      // first part of command="<n>-<mdid1>-<mdid2>"
    std::vector<MenuNode> children;  // Popup only
};

// Menu text without access keys: "&&" is a literal ampersand, a single "&" marks the access key.
std::wstring CleanName(const std::wstring& raw);

// Reads an exported Archicad command-layout scheme (Work Environment) into a menu tree.
class SchemeReader {
public:
    // Top-level menus of the "Nicht-Text" menu bar; empty if missing or malformed.
    static std::vector<MenuNode> ReadFloorPlanMenuBar(const std::string& utf8Xml);
};

} // namespace Baaz
