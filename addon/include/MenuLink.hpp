#pragma once

#include "SchemeReader.hpp"
#include <string>
#include <vector>

namespace Baaz {

struct LiveEntry {
    MenuNodeType type = MenuNodeType::Item;
    unsigned liveId = 0;
    bool enabled = true;
};

// The live Win32 menu bar, behind an interface so linking can be tested without Archicad.
class MenuSource {
public:
    virtual ~MenuSource() = default;
    virtual std::vector<std::wstring> TopLevelTitles() = 0;  // access keys removed
    // path[0] = top-level index, then popup indices; fills the menu before reading it
    virtual std::vector<LiveEntry> Entries(const std::vector<int>& path) = 0;
};

struct LinkedCommand {
    std::wstring name;          // empty if the scheme gave the command no name
    std::wstring menuPath;      // menu titles joined with U+203A, e.g. View or Documentation > Graphic Override
    std::vector<int> position;  // full path including the item's own index
    unsigned liveId = 0;
    bool enabled = true;
    unsigned commandNumber = 0;  // full command id of the scheme item; all zero for entries of a dynamic list
    unsigned mdid1 = 0;
    unsigned mdid2 = 0;
};

// Names for the entries Archicad puts in place of one placeholder item, in menu order
struct DynamicList {
    unsigned placeholder = 0;
    std::vector<std::wstring> names;
};

// Links the scheme's menu tree to the live menu by position. Top-level menus are paired by unique
// title (the rest of either side is ignored); inside a menu the position decides. Whenever a
// position is in doubt, nothing is linked there: a name must never run a different command.
class MenuLink {
public:
    // Assumes equal item counts mean the same items in the same order. A placeholder may expand to
    // any number of live entries, so anchoring around it cannot tell an expanded placeholder from
    // an unexplained extra entry or a missing static item. This relies on a scheme that is as
    // current as the live menu and on callers re-checking the live entry before running it.
    // The add-ons loaded now may differ from those of the export, so a part (the entries between two
    // separators) with a placeholder links nothing if it holds an Item with a non-zero MDID or lost an
    // entry to DropAbsentAddOnItems. Remaining risks, both accepted and not observed in Archicad:
    // - in a part without a placeholder, one add-on item that left since the export and another that
    //   came at the same position keep the count equal;
    // - around a placeholder region whole parts are anchored by position, so a new live part the
    //   scheme does not hold (e.g. an add-on's own part after a separator at the end of a menu) can
    //   take the place of the last scheme part, whose commands then run the new part's entries.
    // notSearchable, if given: the Items (not placeholders) of the top-level menus whose title matched,
    // at any depth, that a new export can make searchable: linked without a name, or not linked
    // because a count differs (a part, a popup, the parts around a placeholder region, a menu whose
    // parts do not match). Not counted: the Items of a part skipped as uncertain (see above), also
    // when its count differs as well, placeholders, menus not in the live bar, items dropped by
    // DropAbsentAddOnItems.
    static std::vector<LinkedCommand> Link(const std::vector<MenuNode>& bar, MenuSource& live,
                                           const std::vector<DynamicList>& lists, size_t* notSearchable = nullptr);
    // -1 if no top-level menu has a title in common with the live bar, or the common ones are in
    // another order; otherwise the number of linked commands
    static int FitScore(const std::vector<MenuNode>& bar, MenuSource& live);
};

// Removes every Item with a non-zero MDID whose (mdid1, mdid2) occurs in no Item of exportBar, then every
// Popup that had an Item or Popup before and has none left. The scheme keeps the configured layout of
// add-ons that are not loaded; the live menu and the export leave those commands out. The entries left in
// a part that lost an entry get partLostEntries.
void DropAbsentAddOnItems(std::vector<MenuNode>& bar, const std::vector<MenuNode>& exportBar);

// Sets the name of every Item of bar whose full command id has an Item in exportBar; others stay as they are.
void AttachNames(std::vector<MenuNode>& bar, const std::vector<MenuNode>& exportBar);

} // namespace Baaz
