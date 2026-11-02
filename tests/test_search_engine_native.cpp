// Keep assert() active in Release builds too
#undef NDEBUG
#include "SearchEngine.hpp"
#include <iostream>
#include <cassert>
#include <chrono>
#include <string>
#include <vector>

using namespace Baaz;

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

int main() {
    std::cout << "Running Native Archicad Search Engine Unit Tests...\n";
    TestEmptyQuery();
    TestExactChords();
    TestChordPrefixBoost();
    TestAlphanumericAndMultiToken();
    TestLevenshteinTypoTolerance();
    TestUnicodeSupport();
    TestPerformance1000Items();
    std::cout << "\nALL 7 SEARCH ENGINE NATIVE TESTS PASSED!\n";
    return 0;
}
