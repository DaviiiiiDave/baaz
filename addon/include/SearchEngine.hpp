#pragma once

#include <string>
#include <vector>

namespace Baaz {

enum class ItemKind { Tool, View, Sheet, LayerCombo, Setting, Help, Command, Favorite };

struct SearchItem {
    std::wstring title;
    std::wstring tag;        // chord for tools, otherwise the label shown on the right
    ItemKind kind = ItemKind::Help;
    std::string guid;        // navigator item or layer combination GUID
    int toolType = 0;        // API_ElemTypeID and API_ElemVariationID of a tool
    int toolVariation = 0;
    double score = 0.0;
    std::vector<int> menuPosition;  // Command: path through the live menu bar, including the item's own index
    unsigned liveId = 0;            // Command: the id the live menu item had when it was linked
    unsigned long commandId = 0;    // Command: key in Archicad's command table
    unsigned long developerId = 0;
    unsigned long productId = 0;
    bool enabled = true;            // Command: false when Archicad reports it disabled
};

class SearchEngine {
public:
    static void Initialize();
    static void SetProjectItems(std::vector<SearchItem> items);
    static void ClearProjectItems();
    // Limit and fuzziness come from Settings; maxResults overrides the limit when > 0.
    static std::vector<SearchItem> Query(const std::wstring& queryStr, size_t maxResults = 0);

    // Chord cheatsheet plus key help, shared by the "?" results and the Settings dialog.
    static std::vector<std::wstring> HelpLines(size_t chordsPerLine);

    static int DamerauLevenshteinDistance(const std::wstring& s1, const std::wstring& s2);

private:
    // Normalized once when items are added, not on every keystroke
    struct IndexedItem {
        SearchItem item;
        std::wstring cleanTitle;
        std::vector<std::wstring> tokens;
    };

    // True when the tool table has a row for this element type and variation; the Arrow row never counts
    static bool HasTool(int typeId, int variation);
    static IndexedItem MakeIndexed(SearchItem item);
    static std::vector<std::wstring> Tokenize(const std::wstring& str);

    static std::vector<IndexedItem> s_tools;
    static std::vector<IndexedItem> s_projectItems;
};

} // namespace Baaz
