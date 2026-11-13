#pragma once

#include "SearchEngine.hpp"
#include <string>
#include <vector>

namespace Baaz {

// One row of Archicad's command table, copied out so the rules below need no Archicad API
struct CommandEntry {
    unsigned long commandId = 0;
    unsigned long developerId = 0;
    unsigned long productId = 0;
    std::wstring name;       // display text, without '&'
    std::wstring category;   // Work Environment group, e.g. "04 Ausschnitt"
    bool visible = true;
    bool enabled = true;
};

class CommandCatalog {
public:
    // True for rows worth searching: visible, named, and not one of Archicad's internal id ranges
    static bool IsOffered(const CommandEntry& entry);
    // The group name without its sort prefix ("04 Ausschnitt" -> "Ausschnitt")
    static std::wstring CategoryLabel(const std::wstring& category);
    static std::vector<SearchItem> MakeItems(const std::vector<CommandEntry>& table);
    // True when the table still holds the item's row (same three ids), offered and enabled
    static bool IsRunnable(const std::vector<CommandEntry>& table, const SearchItem& item);
};

} // namespace Baaz
