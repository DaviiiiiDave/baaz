#include "CommandCatalog.hpp"
#include <utility>

namespace Baaz {

struct IdRange {
    unsigned long first;
    unsigned long last;
};

// Archicad's own ids that are no commands a user would run (separators, menu stubs, internal slots)
static const IdRange kInternalRanges[] = {
    { 257, 266 }, { 13825, 14000 }, { 19201, 19224 }, { 30465, 30471 },
};

// Inside those ranges, but real commands
static const unsigned long kInternalRangeExceptions[] = { 13983, 13984, 13988 };

static bool IsInternalId(unsigned long commandId) {
    for (unsigned long id : kInternalRangeExceptions) {
        if (commandId == id) return false;
    }
    for (const IdRange& range : kInternalRanges) {
        if (commandId >= range.first && commandId <= range.last) return true;
    }
    return false;
}

bool CommandCatalog::IsOffered(const CommandEntry& entry) {
    if (!entry.visible || entry.name.empty()) return false;
    // Add-ons number their commands themselves, so only Archicad's own ids are filtered
    const bool archicadOwn = (entry.developerId == 0 && entry.productId == 0);
    return !(archicadOwn && IsInternalId(entry.commandId));
}

std::wstring CommandCatalog::CategoryLabel(const std::wstring& category) {
    const bool hasSortPrefix = category.size() >= 3
        && category[0] >= L'0' && category[0] <= L'9'
        && category[1] >= L'0' && category[1] <= L'9'
        && category[2] == L' ';
    return hasSortPrefix ? category.substr(3) : category;
}

std::vector<SearchItem> CommandCatalog::MakeItems(const std::vector<CommandEntry>& table) {
    std::vector<SearchItem> items;
    for (const CommandEntry& entry : table) {
        if (!IsOffered(entry)) continue;
        SearchItem item;
        item.title = entry.name;
        item.tag = CategoryLabel(entry.category);
        item.kind = ItemKind::Command;
        item.commandId = entry.commandId;
        item.developerId = entry.developerId;
        item.productId = entry.productId;
        item.enabled = entry.enabled;
        items.push_back(std::move(item));
    }
    return items;
}

bool CommandCatalog::IsRunnable(const std::vector<CommandEntry>& table, const SearchItem& item) {
    for (const CommandEntry& entry : table) {
        if (entry.commandId == item.commandId && entry.developerId == item.developerId
            && entry.productId == item.productId) {
            return IsOffered(entry) && entry.enabled;
        }
    }
    return false;
}

} // namespace Baaz
