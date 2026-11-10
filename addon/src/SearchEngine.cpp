#include "SearchEngine.hpp"
#include "Settings.hpp"
#include <algorithm>
#include <cmath>
#include <cwctype>

namespace Baaz {

std::vector<SearchEngine::IndexedItem> SearchEngine::s_tools;
std::vector<SearchEngine::IndexedItem> SearchEngine::s_projectItems;

static std::wstring ToLower(std::wstring str) {
    std::transform(str.begin(), str.end(), str.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return str;
}

static std::wstring ToUpper(std::wstring str) {
    std::transform(str.begin(), str.end(), str.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towupper(c)); });
    return str;
}

static std::wstring Join(const std::vector<std::wstring>& tokens) {
    std::wstring joined;
    for (const auto& token : tokens) joined += token;
    return joined;
}

// Lower-cased alphanumeric runs; everything else separates tokens
std::vector<std::wstring> SearchEngine::Tokenize(const std::wstring& str) {
    std::vector<std::wstring> tokens;
    std::wstring current;
    for (wchar_t ch : str) {
        if (std::iswalnum(ch)) {
            current.push_back(static_cast<wchar_t>(std::towlower(ch)));
        } else if (!current.empty()) {
            tokens.push_back(std::move(current));
            current.clear();
        }
    }
    if (!current.empty()) {
        tokens.push_back(std::move(current));
    }
    return tokens;
}

int SearchEngine::DamerauLevenshteinDistance(const std::wstring& s1, const std::wstring& s2) {
    const size_t len1 = s1.length();
    const size_t len2 = s2.length();
    const size_t cols = len2 + 1;

    // Reused across calls: this runs for every token pair on every keystroke
    static thread_local std::vector<int> d;
    d.assign((len1 + 1) * cols, 0);
    auto at = [&](size_t i, size_t j) -> int& { return d[i * cols + j]; };

    for (size_t i = 0; i <= len1; ++i) at(i, 0) = static_cast<int>(i);
    for (size_t j = 0; j <= len2; ++j) at(0, j) = static_cast<int>(j);

    for (size_t i = 1; i <= len1; ++i) {
        for (size_t j = 1; j <= len2; ++j) {
            int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
            int dist = (std::min)({ at(i - 1, j) + 1, at(i, j - 1) + 1, at(i - 1, j - 1) + cost });
            if (i > 1 && j > 1 && s1[i - 1] == s2[j - 2] && s1[i - 2] == s2[j - 1]) {
                dist = (std::min)(dist, at(i - 2, j - 2) + 1); // transposition
            }
            at(i, j) = dist;
        }
    }
    return at(len1, len2);
}

SearchEngine::IndexedItem SearchEngine::MakeIndexed(SearchItem item) {
    IndexedItem indexed;
    indexed.tokens = Tokenize(item.title);
    indexed.cleanTitle = Join(indexed.tokens);
    indexed.item = std::move(item);
    return indexed;
}

void SearchEngine::Initialize() {
    s_tools.clear();

    // Every tool in Archicad's Toolbox that the API can activate, as
    // API_ElemTypeID and API_ElemVariationID values so this table stays free of
    // ACAPI headers.
    struct ToolDef {
        const wchar_t* title;
        const wchar_t* chord;
        int typeId;
        int variation;
    };

    static const ToolDef tools[] = {
        // Design
        { L"Wall Tool",                L"wa", 1,  0 },
        { L"Door Tool",                L"dr", 5,  0 },
        { L"Window Tool",              L"wn", 4,  0 },
        { L"Slab Tool",                L"sl", 8,  0 },
        { L"Column Tool",              L"co", 2,  0 },
        { L"Beam Tool",                L"bm", 3,  0 },
        { L"Curtain Wall Tool",        L"cw", 37, 0 },
        { L"Stair Tool",               L"st", 47, 0 },
        { L"Railing Tool",             L"ra", 51, 0 },
        { L"Roof Tool",                L"ro", 9,  0 },
        { L"Shell Tool",               L"sh", 43, 0 },
        { L"Skylight Tool",            L"sk", 44, 0 },
        { L"Corner Window Tool",       L"cn", 4,  'CRWD' },
        { L"Wall End Tool",            L"we", 4,  'WAEN' },
        { L"Object Tool",              L"ob", 6,  0 },
        { L"Lamp Tool",                L"la", 7,  0 },
        { L"Zone Tool",                L"zo", 17, 0 },
        { L"Mesh Tool",                L"me", 10, 0 },
        { L"Morph Tool",               L"mo", 45, 0 },
        { L"Opening Tool",             L"op", 72, 0 },
        { L"Grid Element Tool",        L"gr", 6,  'GREL' },
        // Viewpoints
        { L"Section Tool",             L"se", 25, 0 },
        { L"Elevation Tool",           L"el", 33, 0 },
        { L"Interior Elevation Tool",  L"ie", 34, 0 },
        { L"Worksheet Tool",           L"ws", 35, 0 },
        { L"Detail Tool",              L"de", 32, 0 },
        { L"Camera Tool",              L"ca", 26, 0 },
        // Document
        { L"Linear Dimension Tool",    L"di", 11, 0 },
        { L"Level Dimension Tool",     L"ld", 13, 0 },
        { L"Radial Dimension Tool",    L"rd", 12, 0 },
        { L"Angle Dimension Tool",     L"ad", 14, 0 },
        { L"Text Tool",                L"tx", 15, 0 },
        { L"Label Tool",               L"lb", 16, 0 },
        { L"Fill Tool",                L"fi", 18, 0 },
        { L"Line Tool",                L"li", 19, 0 },
        { L"Polyline Tool",            L"pl", 20, 0 },
        { L"Arc/Circle Tool",          L"ac", 21, 0 },
        { L"Spline Tool",              L"sp", 23, 0 },
        { L"Hotspot Tool",             L"ho", 24, 0 },
        { L"Change Marker Tool",       L"cm", 46, 0 },
        { L"Drawing Tool",             L"dw", 30, 0 },
        { L"Figure Tool",              L"pi", 31, 0 },
        // Arrow mode is what the API sets for element type 0
        { L"Arrow (Selection) Tool",   L"ar", 0,  0 }
    };

    for (const auto& def : tools) {
        SearchItem item;
        item.title = def.title;
        item.tag = def.chord;
        item.kind = ItemKind::Tool;
        item.toolType = def.typeId;
        item.toolVariation = def.variation;
        s_tools.push_back(MakeIndexed(std::move(item)));
    }
}

bool SearchEngine::HasTool(int typeId, int variation) {
    return typeId != 0 && std::any_of(s_tools.begin(), s_tools.end(), [&](const IndexedItem& tool) {
        return tool.item.toolType == typeId && tool.item.toolVariation == variation;
    });
}

void SearchEngine::SetProjectItems(std::vector<SearchItem> items) {
    s_projectItems.clear();
    s_projectItems.reserve(items.size());
    for (auto& item : items) {
        // Components, MEP and other favorites without a tool of their own cannot be placed from here
        if (item.kind == ItemKind::Favorite && !HasTool(item.toolType, item.toolVariation)) continue;
        s_projectItems.push_back(MakeIndexed(std::move(item)));
    }
}

void SearchEngine::ClearProjectItems() {
    s_projectItems.clear();
}

std::vector<std::wstring> SearchEngine::HelpLines(size_t chordsPerLine) {
    std::vector<std::wstring> lines;
    std::wstring line;
    size_t onLine = 0;
    for (const auto& tool : s_tools) {
        std::wstring name = tool.item.title;
        size_t suffix = name.rfind(L" Tool");
        if (suffix != std::wstring::npos) name.erase(suffix);
        if (onLine > 0) line += L"   ";
        line += ToUpper(tool.item.tag) + L" " + name;
        if (++onLine == chordsPerLine) {
            lines.push_back(std::move(line));
            line.clear();
            onLine = 0;
        }
    }
    if (onLine > 0) lines.push_back(std::move(line));

    lines.push_back(L"Space tap: open/close HUD  \x2022  Hold " + std::to_wstring(Settings::Get().spacebarHoldMs) + L" ms: Magic Wand");
    lines.push_back(L"Type to search views, sheets, layer combinations, favorites and tools  \x2022  ? help  \x2022  :settings");
    lines.push_back(L"\x2191/\x2193 select  \x2022  Space/Enter run  \x2022  Shift+Space types a space  \x2022  Esc close");
    return lines;
}

// On equal scores a favorite is what the user can place next, so it follows the tools
static int KindRank(ItemKind kind) {
    if (kind == ItemKind::Tool) return 0;
    if (kind == ItemKind::Favorite) return 1;
    return 2;
}

static SearchItem MakeSettingsItem(const wchar_t* title) {
    SearchItem item;
    item.title = title;
    item.tag = L"CONFIG";
    item.kind = ItemKind::Setting;
    item.score = 100.0;
    return item;
}

std::vector<SearchItem> SearchEngine::Query(const std::wstring& queryStr, size_t maxResults) {
    const SettingsConfig& settings = Settings::Get();
    const size_t limit = (maxResults > 0) ? maxResults : settings.maxResults;

    if (queryStr.empty()) {
        std::vector<SearchItem> results;
        for (size_t i = 0; i < s_tools.size() && i < limit; ++i) {
            results.push_back(s_tools[i].item);
            results.back().score = 100.0 - static_cast<double>(i);
        }
        return results;
    }

    const std::wstring lowerQuery = ToLower(queryStr);

    if (lowerQuery == L"?" || lowerQuery == L"help") {
        std::vector<SearchItem> results = { MakeSettingsItem(L"Open Settings & Preferences Dialog") };
        for (auto& line : HelpLines(5)) {
            SearchItem item;
            item.title = std::move(line);
            item.tag = L"HELP";
            item.kind = ItemKind::Help;
            item.score = 90.0;
            results.push_back(std::move(item));
        }
        return results;
    }

    if (lowerQuery == L":settings" || lowerQuery == L"settings" || lowerQuery == L"config" || lowerQuery == L":config") {
        return { MakeSettingsItem(L"Open Settings & Help Dialog (Timings, Chords, Display)") };
    }

    const std::vector<std::wstring> queryTokens = Tokenize(lowerQuery);
    const std::wstring cleanQuery = Join(queryTokens);

    auto score = [&](const IndexedItem& entry) -> double {
        const SearchItem& item = entry.item;
        const bool isTool = (item.kind == ItemKind::Tool);

        // Tier 1: exact chord, Tier 1.5: single letter matching a chord's first letter
        if (settings.enableChords && isTool) {
            if (item.tag == lowerQuery) return 100.0;
            if (lowerQuery.length() == 1 && !item.tag.empty() && item.tag[0] == lowerQuery[0]) return 98.0;
        }

        // Tier 2: alphanumeric match, ignoring punctuation and spaces
        if (!cleanQuery.empty() && !entry.cleanTitle.empty()) {
            if (entry.cleanTitle == cleanQuery) return 95.0;
            if (cleanQuery.length() >= 2 && entry.cleanTitle.find(cleanQuery) != std::wstring::npos) return 93.0;
            if (cleanQuery.length() == 1 && entry.cleanTitle.rfind(cleanQuery, 0) == 0) return 93.0;
        }
        if (queryTokens.empty() || entry.tokens.empty()) return 0.0;

        // Tier 3: every query token is a prefix of some title token
        bool allTokensPrefixMatch = std::all_of(queryTokens.begin(), queryTokens.end(), [&](const std::wstring& qTok) {
            return std::any_of(entry.tokens.begin(), entry.tokens.end(),
                               [&](const std::wstring& tTok) { return tTok.rfind(qTok, 0) == 0; });
        });
        if (allTokensPrefixMatch) return 91.0;

        // Tier 4: token-level Damerau-Levenshtein typo match, capped at 89
        double totalTokenScore = 0.0;
        for (const auto& qTok : queryTokens) {
            double best = 0.0;
            for (const auto& tTok : entry.tokens) {
                const double maxLen = static_cast<double>((std::max)(qTok.length(), tTok.length()));
                const double lengthGap = std::fabs(static_cast<double>(qTok.length()) - static_cast<double>(tTok.length()));
                if ((1.0 - lengthGap / maxLen) * 89.0 <= best) continue; // can't beat the best so far
                int dist = DamerauLevenshteinDistance(qTok, tTok);
                best = (std::max)(best, (1.0 - dist / maxLen) * 89.0);
            }
            totalTokenScore += best;
        }
        return totalTokenScore / static_cast<double>(queryTokens.size());
    };

    struct Candidate { double score; const IndexedItem* entry; };
    std::vector<Candidate> candidates;
    for (const auto* pool : { &s_tools, &s_projectItems }) {
        for (const auto& entry : *pool) {
            double s = score(entry);
            if (s >= settings.fuzzThreshold) candidates.push_back({ s, &entry });
        }
    }

    // Score desc, then tools, favorites, everything else, then title
    auto better = [](const Candidate& a, const Candidate& b) {
        if (std::abs(a.score - b.score) > 0.001) return a.score > b.score;
        const int aRank = KindRank(a.entry->item.kind);
        const int bRank = KindRank(b.entry->item.kind);
        if (aRank != bRank) return aRank < bRank;
        return a.entry->item.title < b.entry->item.title;
    };
    const size_t count = (std::min)(limit, candidates.size());
    std::partial_sort(candidates.begin(), candidates.begin() + count, candidates.end(), better);

    std::vector<SearchItem> results;
    results.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        results.push_back(candidates[i].entry->item);
        results.back().score = candidates[i].score;
    }
    return results;
}

} // namespace Baaz
