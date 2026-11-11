#include "NavigatorIndexer.hpp"

#ifdef ACAPI_VERSION
#include "APIEnvir.h"
#include "ACAPinc.h"
#include "ACAPI/Favorite.hpp"
#endif

namespace Baaz {

#ifdef ACAPI_VERSION

static std::wstring ToWString(const GS::UniString& str) {
    return reinterpret_cast<const wchar_t*>(str.ToUStr().Get());
}

// Folders and other containers hold views or layouts but can't be opened
static bool IsContainer(API_NavigatorItemTypeID type) {
    switch (type) {
    case API_UndefinedNavItem:
    case API_ProjectNavItem:
    case API_CameraSetNavItem:
    case API_InfoNavItem:
    case API_HelpNavItem:
    case API_BookNavItem:
    case API_MasterFolderNavItem:
    case API_SubSetNavItem:
    case API_FolderNavItem:
        return true;
    default:
        return false;
    }
}

// Prefix the Navigator ID (e.g. "A.01") so sheets and views can be found by number
static std::wstring NavigatorTitle(const API_NavigatorItem& navItem) {
    GS::UniString name(navItem.uName);
    GS::UniString id(navItem.uiId);
    if (id.IsEmpty() || name.BeginsWith(id)) return ToWString(name);
    return ToWString(id + " " + name);
}

static void CollectNavigatorItems(API_NavigatorMapID mapId, const API_Guid& parentGuid, ItemKind kind,
                                  const wchar_t* tag, std::vector<SearchItem>& items) {
    API_NavigatorItem parent = {};
    parent.guid = parentGuid;
    parent.mapId = mapId;
    GS::Array<API_NavigatorItem> children;
    if (ACAPI_Navigator_GetNavigatorChildrenItems(&parent, &children) != NoError) return;

    for (const API_NavigatorItem& child : children) {
        if (IsContainer(child.itemType)) {
            CollectNavigatorItems(mapId, child.guid, kind, tag, items);
            continue;
        }
        SearchItem item;
        item.title = NavigatorTitle(child);
        item.tag = tag;
        item.kind = kind;
        item.guid = APIGuidToString(child.guid).ToCStr().Get();
        items.push_back(std::move(item));
    }
}

static void CollectNavigatorMap(API_NavigatorMapID mapId, ItemKind kind, const wchar_t* tag, std::vector<SearchItem>& items) {
    API_NavigatorSet set = {};
    set.mapId = mapId;
    if (ACAPI_Navigator_GetNavigatorSet(&set) == NoError) {
        CollectNavigatorItems(mapId, set.rootGuid, kind, tag, items);
    }
}

static void CollectLayerCombinations(std::vector<SearchItem>& items) {
    GS::Array<API_Attribute> layerCombs;
    if (ACAPI_Attribute_GetAttributesByType(API_LayerCombID, layerCombs) != NoError) return;

    for (const API_Attribute& comb : layerCombs) {
        // Re-read to get the full Unicode name
        GS::UniString name;
        API_Attribute attr = {};
        attr.header.typeID = API_LayerCombID;
        attr.header.index = comb.header.index;
        attr.header.uniStringNamePtr = &name;
        if (ACAPI_Attribute_Get(&attr) != NoError) continue;

        SearchItem item;
        item.title = ToWString(name);
        item.tag = L"LAYERS";
        item.kind = ItemKind::LayerCombo;
        item.guid = APIGuidToString(attr.header.guid).ToCStr().Get();
        items.push_back(std::move(item));
    }
}

// Only the innermost folder fits the right column. FAV means the top level; ? means the folder
// could not be read, so a failed lookup is never shown as a top-level favorite.
static std::wstring FavoriteFolderText(const ACAPI::Favorite::Favorite& favorite) {
    const auto parent = favorite.GetParentFolder();
    if (parent.IsErr()) return L"?";
    const auto isRoot = parent.Unwrap().IsRoot();
    if (isRoot.IsErr()) return L"?";
    if (isRoot.Unwrap()) return L"FAV";
    const auto folderName = parent.Unwrap().GetName();
    return folderName.IsOk() ? ToWString(folderName.Unwrap()) : L"?";
}

// Which of these can be placed is decided by SearchEngine, which knows the tool table
static void CollectFavorites(std::vector<SearchItem>& items) {
    const size_t before = items.size();
    try {
        for (const ACAPI::Favorite::Favorite& favorite : ACAPI::Favorite::GetFavoriteManager().GetAllFavorites()) {
            if (!favorite.IsValid()) continue;
            const auto name = favorite.GetName();
            const auto type = favorite.GetElementType();
            if (name.IsErr() || type.IsErr()) continue;

            SearchItem item;
            item.title = ToWString(name.Unwrap());
            item.tag = FavoriteFolderText(favorite);
            item.kind = ItemKind::Favorite;
            item.toolType = static_cast<int>(type.Unwrap().typeID);
            item.toolVariation = static_cast<int>(type.Unwrap().variationID);
            items.push_back(std::move(item));
        }
    } catch (...) {
        // A throwing favorite list must not take views and layouts with it
        items.resize(before);
    }
}

bool NavigatorIndexer::HasOpenProject() {
    API_ProjectInfo projectInfo = {};
    return (ACAPI_ProjectOperation_Project(&projectInfo) == NoError);
}

std::vector<SearchItem> NavigatorIndexer::BuildProjectIndex() {
    std::vector<SearchItem> items;
    CollectNavigatorMap(API_PublicViewMap, ItemKind::View, L"VIEW", items);
    CollectNavigatorMap(API_LayoutMap, ItemKind::Sheet, L"SHEET", items);
    CollectLayerCombinations(items);
    CollectFavorites(items);
    return items;
}

#else

// Without Archicad (tests) a project is always open and has no items
bool NavigatorIndexer::HasOpenProject() { return true; }
std::vector<SearchItem> NavigatorIndexer::BuildProjectIndex() { return {}; }

#endif

} // namespace Baaz
