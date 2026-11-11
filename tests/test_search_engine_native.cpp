// Keep assert() active in Release builds too
#undef NDEBUG
#include "SearchEngine.hpp"
#include <iostream>
#include <cassert>
#include <chrono>
#include <string>
#include <vector>

using namespace Baaz;

static SearchItem Favorite(const wchar_t* name, int typeId, int variation, const wchar_t* folder = L"FAV") {
    SearchItem item;
    item.title = name;
    item.tag = folder;
    item.kind = ItemKind::Favorite;
    item.toolType = typeId;
    item.toolVariation = variation;
    return item;
}

void TestEmptyQuery() {
    SearchEngine::Initialize();
    auto results = SearchEngine::Query(L"");
    assert(results.size() == 6);
    assert(results[0].title == L"Wall Tool");
    assert(results[0].tag == L"wa");
    assert(results[0].kind == ItemKind::Tool);
    std::cout << "[PASS] TestEmptyQuery\n";
}

void TestExactChords() {
    SearchEngine::Initialize();
    
    auto resWA = SearchEngine::Query(L"wa");
    assert(!resWA.empty());
    assert(resWA[0].title == L"Wall Tool");
    assert(resWA[0].score == 100.0);

    auto resDR = SearchEngine::Query(L"dr");
    assert(!resDR.empty());
    assert(resDR[0].title == L"Door Tool");
    assert(resDR[0].score == 100.0);

    auto resSL = SearchEngine::Query(L"sl");
    assert(!resSL.empty());
    assert(resSL[0].title == L"Slab Tool");
    assert(resSL[0].score == 100.0);

    std::cout << "[PASS] TestExactChords\n";
}

void TestChordPrefixBoost() {
    SearchEngine::Initialize();

    std::vector<SearchItem> projectItems = {
        { L"West Elevation View", L"VIEW", ItemKind::View, "GUID_WEST" }
    };
    SearchEngine::SetProjectItems(projectItems);

    auto results = SearchEngine::Query(L"w");
    assert(results.size() >= 2);
    // Wall and Window should score 98.0 and rank at top
    assert(results[0].kind == ItemKind::Tool);
    assert(results[0].score == 98.0);
    assert(results[1].kind == ItemKind::Tool);
    assert(results[1].score == 98.0);

    std::cout << "[PASS] TestChordPrefixBoost\n";
}

void TestAlphanumericAndMultiToken() {
    SearchEngine::Initialize();

    std::vector<SearchItem> projectItems = {
        { L"A-101 Submission Layout", L"SHEET", ItemKind::Sheet, "GUID_A101" },
        { L"01 Ground Floor GFC Masonry Plan", L"VIEW", ItemKind::View, "GUID_GFC" }
    };
    SearchEngine::SetProjectItems(projectItems);

    // Alphanumeric search
    auto res101 = SearchEngine::Query(L"a101");
    assert(!res101.empty());
    assert(res101[0].title == L"A-101 Submission Layout");
    assert(res101[0].score >= 93.0);

    // Multi-token prefix search
    auto resGfc = SearchEngine::Query(L"gfc mas");
    assert(!resGfc.empty());
    assert(resGfc[0].title == L"01 Ground Floor GFC Masonry Plan");
    assert(resGfc[0].score >= 91.0);

    std::cout << "[PASS] TestAlphanumericAndMultiToken\n";
}

void TestLevenshteinTypoTolerance() {
    SearchEngine::Initialize();

    std::vector<SearchItem> projectItems = {
        { L"01 Ground Floor GFC Masonry Plan", L"VIEW", ItemKind::View, "GUID_GFC" }
    };
    SearchEngine::SetProjectItems(projectItems);

    // Typo: "masnry" instead of "masonry"
    auto results = SearchEngine::Query(L"masnry");
    assert(!results.empty());
    assert(results[0].title == L"01 Ground Floor GFC Masonry Plan");
    assert(results[0].score >= 60.0);

    std::cout << "[PASS] TestLevenshteinTypoTolerance\n";
}

void TestUnicodeSupport() {
    SearchEngine::Initialize();

    std::vector<SearchItem> projectItems = {
        { L"02 Büro Grundriss", L"VIEW", ItemKind::View, "GUID_BURO" }
    };
    SearchEngine::SetProjectItems(projectItems);

    auto results = SearchEngine::Query(L"büro");
    assert(!results.empty());
    assert(results[0].title == L"02 Büro Grundriss");
    assert(results[0].score >= 93.0);

    std::cout << "[PASS] TestUnicodeSupport\n";
}

void TestPerformance1000Items() {
    SearchEngine::Initialize();

    std::vector<SearchItem> largeProject;
    largeProject.reserve(1000);
    for (int i = 0; i < 1000; ++i) {
        SearchItem item;
        item.title = L"View " + std::to_wstring(i) + L" Level " + std::to_wstring(i % 10) + L" Structural Plan";
        item.tag = L"VIEW";
        item.kind = ItemKind::View;
        item.guid = "GUID_" + std::to_string(i);
        largeProject.push_back(item);
    }
    SearchEngine::SetProjectItems(largeProject);

    auto start = std::chrono::high_resolution_clock::now();
    auto results = SearchEngine::Query(L"struct 50");
    auto end = std::chrono::high_resolution_clock::now();
    double elapsedMs = std::chrono::duration<double, std::milli>(end - start).count();

    assert(!results.empty());
    std::cout << "[PASS] TestPerformance1000Items (Completed in " << elapsedMs << " ms)\n";
#ifdef _DEBUG
    assert(elapsedMs < 100.0); // unoptimized build
#else
    assert(elapsedMs < 10.0); // well under real-time threshold
#endif
}

void TestFavoritesWithoutToolAreDropped() {
    SearchEngine::Initialize();
    SearchEngine::SetProjectItems({
        Favorite(L"KS 24 cm", 1, 0),                         // Wall: listed
        Favorite(L"Eckfenster Holz", 4, 'CRWD'),             // Corner Window: listed
        Favorite(L"Metall - Handlauf", 52, 0),               // railing component: dropped
        Favorite(L"Kabeltrasse", 0, 0),                      // no type: dropped
        Favorite(L"Fenster Sonder", 4, 12345),               // Window type, unknown variation: dropped
    });
    auto favoritesIn = [](const std::wstring& query) {
        size_t n = 0;
        for (const auto& r : SearchEngine::Query(query, 50)) n += (r.kind == ItemKind::Favorite);
        return n;
    };
    assert(favoritesIn(L"ks 24") == 1);
    assert(favoritesIn(L"eckfenster") == 1);
    assert(favoritesIn(L"handlauf") == 0);
    assert(favoritesIn(L"kabeltrasse") == 0);
    assert(favoritesIn(L"sonder") == 0);
    std::cout << "[PASS] TestFavoritesWithoutToolAreDropped\n";
}

void TestFavoriteRanksAfterToolBeforeView() {
    SearchEngine::Initialize();
    SearchEngine::SetProjectItems({
        { L"Decke EG", L"VIEW", ItemKind::View, "GUID_DECKE" },
        Favorite(L"Decke STB 20", 8, 0, L"Decken+Böden"),
        Favorite(L"Wallpaper", 1, 0),
    });
    auto decke = SearchEngine::Query(L"decke");
    assert(decke.size() >= 2);
    assert(decke[0].kind == ItemKind::Favorite && decke[0].title == L"Decke STB 20");
    assert(decke[1].kind == ItemKind::View);
    assert(decke[0].tag == L"Decken+Böden");

    // "Curtain Wall Tool", "Wall End Tool", "Wall Tool" and "Wallpaper" all score 93
    auto wall = SearchEngine::Query(L"wall", 20);
    size_t fav = 0;
    while (fav < wall.size() && wall[fav].kind != ItemKind::Favorite) ++fav;
    assert(fav > 0 && fav < wall.size());
    for (size_t i = 0; i < fav; ++i) assert(wall[i].kind == ItemKind::Tool);

    assert(SearchEngine::Query(L"wa")[0].title == L"Wall Tool");   // exact chord still wins
    std::cout << "[PASS] TestFavoriteRanksAfterToolBeforeView\n";
}

void TestFavoriteFoundByWords() {
    SearchEngine::Initialize();
    SearchEngine::SetProjectItems({ Favorite(L"KS + WDVS 42 cm", 1, 0, L"Wände") });
    auto results = SearchEngine::Query(L"ks wdvs");
    assert(!results.empty());
    assert(results[0].title == L"KS + WDVS 42 cm");
    std::cout << "[PASS] TestFavoriteFoundByWords\n";
}

void TestWhitespacePaddedChordKeepsExactChordRank() {
    SearchEngine::Initialize();
    SearchEngine::SetProjectItems({ Favorite(L"Wallpaper", 1, 0) });
    // Shift+Space makes a leading space reachable; the chord must still win over "Curtain Wall Tool"
    assert(SearchEngine::Query(L" wa")[0].title == L"Wall Tool");
    assert(SearchEngine::Query(L"wa ")[0].title == L"Wall Tool");
    assert(SearchEngine::Query(L" wa ")[0].score == 100.0);
    std::cout << "[PASS] TestWhitespacePaddedChordKeepsExactChordRank\n";
}

void TestWhitespaceOnlyQueryReturnsNothing() {
    SearchEngine::Initialize();
    SearchEngine::SetProjectItems({ Favorite(L"KS 24 cm", 1, 0) });
    assert(SearchEngine::Query(L" ").empty());
    assert(SearchEngine::Query(L"   ").empty());
    std::cout << "[PASS] TestWhitespaceOnlyQueryReturnsNothing\n";
}

int main() {
    std::cout << "Running Native Archicad Search Engine Unit Tests...\n";
    TestEmptyQuery();
    TestExactChords();
    TestChordPrefixBoost();
    TestAlphanumericAndMultiToken();
    TestLevenshteinTypoTolerance();
    TestUnicodeSupport();
    TestPerformance1000Items();
    TestFavoritesWithoutToolAreDropped();
    TestFavoriteRanksAfterToolBeforeView();
    TestFavoriteFoundByWords();
    TestWhitespacePaddedChordKeepsExactChordRank();
    TestWhitespaceOnlyQueryReturnsNothing();
    std::cout << "\nALL 12 SEARCH ENGINE NATIVE TESTS PASSED!\n";
    return 0;
}
