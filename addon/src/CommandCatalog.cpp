#include "CommandCatalog.hpp"
#include <algorithm>
#include <iterator>
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

// Archicad's "Individuelles Kontext-Menue" group. Its ids are scattered, so a range cannot
// describe it. These entries act on context the user does not see (tab bar, views, energy) and
// several stay disabled outside the right-click menu. Sorted, for binary_search.
static const unsigned long kContextMenuIds[] = {
    2339, 2340, 2341, 2344, 2345, 2355, 2364, 2366, 8197, 9985, 9989, 9990, 9991, 9992, 9993, 9994,
    9995, 9996, 9997, 9999, 10000, 10005, 10006, 10009, 10010, 10012, 10013, 10014, 10015, 10016,
    10017, 10021, 10022, 10024, 10025, 10026, 10030, 10031, 10032, 10033, 10035, 10037, 10038,
    10039, 10040, 10041, 10042, 10043, 10044, 10045, 10046, 10047, 10049, 10052, 10053, 10054,
    10055, 10056, 10057, 10058, 10059, 10061, 10062, 10063, 10064, 10065, 10066, 10067, 10068,
    10069, 10070, 10071, 10072, 10073, 10074, 10075, 10076, 10078, 10079, 10080, 10081, 10082,
    10083, 19464, 19468,
};

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
    if (!archicadOwn) return true;
    return !IsInternalId(entry.commandId)
        && !std::binary_search(std::begin(kContextMenuIds), std::end(kContextMenuIds), entry.commandId);
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
