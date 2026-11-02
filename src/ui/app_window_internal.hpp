#pragma once
#include "app_window.hpp"
#include "theme.hpp"
#include "i18n.hpp"
#include "progression_parser.hpp"
#include "midi_builder.hpp"
#include "ole_dnd.hpp"
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

namespace clp {

#define IDM_FILE_NEW         1001
#define IDM_FILE_OPEN        1002
#define IDM_FILE_DASHBOARD   1007
#define IDM_FILE_SAVE        1003
#define IDM_FILE_SAVEAS      1004
#define IDM_FILE_EXPORT_MIDI 1005
#define IDM_FILE_EXIT        1006
#define IDM_FILE_MRU_BASE    1100 // 1100..1110 for MRU items

#define IDM_EDIT_UNDO        1011
#define IDM_EDIT_REDO        1012
#define IDM_EDIT_DUPLICATE   1013
#define IDM_EDIT_DELETE      1014
#define IDM_EDIT_SELECTALL   1015
#define IDM_EDIT_CLEARALL    1016
#define IDM_EDIT_SPLIT       1017

#define IDM_VIEW_PIANOROLL   1021
#define IDM_VIEW_RIGHTPANE   1022
#define IDM_VIEW_ZOOMIN      1023
#define IDM_VIEW_ZOOMOUT     1024
#define IDM_VIEW_ZOOMRESET   1025

#define IDM_PLAY_PLAYPAUSE   1031
#define IDM_PLAY_STOP        1032
#define IDM_PLAY_LOOP        1033

#define IDM_TOOL_TEMPLATES   1041
#define IDM_TOOL_SETTINGS    1042
#define IDM_TOOL_HELP        1043
#define IDM_TOOL_MERGE_NEW   1044

// プルダウンメニュー用コマンドID
#define IDM_KEY_BASE         3000
#define IDM_SCALE_BASE       3100
#define IDM_VOICING_BASE     3200
#define IDM_BASS_BASE        3300
#define IDM_GRID_BASE        3400
#define IDM_PRESET_BASE      3500
#define IDM_CTX_PART_DUP     3601
#define IDM_CTX_PART_DEL     3602
#define IDM_CTX_PART_LEFT    3603
#define IDM_CTX_PART_RIGHT   3604
#define IDM_CTX_PART_RENAME  3605
#define IDM_CTX_CARD_INV0    3700
#define IDM_CTX_CARD_INV1    3701
#define IDM_CTX_CARD_INV2    3702
#define IDM_CTX_CARD_INV3    3703
#define IDM_CTX_CARD_BASS_NONE 3710
#define IDM_CTX_CARD_BASS_3RD  3711
#define IDM_CTX_CARD_BASS_5TH  3712
#define IDM_CTX_CARD_BASS_7TH  3713
#define IDM_CTX_CARD_BASS_BASE 3720

#define IDC_TEXT_EDIT        2001
#define TIMER_ID_AUTOBACKUP  2050

inline std::wstring ToWString(std::string_view str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), size);
    return result;
}

inline std::string ToUtf8String(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), result.data(), size, nullptr, nullptr);
    return result;
}

inline bool PtInRectF(const D2D1_RECT_F& rc, float x, float y) {
    return (x >= rc.left && x <= rc.right && y >= rc.top && y <= rc.bottom);
}

inline std::string StripLeadingEmoji(std::string_view s) {
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c == ' ') {
            ++i;
            break;
        }
        if (c >= 0xF0 || (i + 2 < s.size() && c == 0xE2) || (i + 2 < s.size() && c == 0xEF)) {
            if (c >= 0xF0) i += 4;
            else i += 3;
        } else {
            break;
        }
    }
    while (i < s.size() && s[i] == ' ') ++i;
    return (i < s.size()) ? std::string(s.substr(i)) : std::string(s);
}

inline std::wstring GetLocalizedCategoryName(std::string_view rawCat, bool isEn) {
    std::string stripped = StripLeadingEmoji(rawCat);
    if (!isEn) return ToWString(stripped);
    if (stripped == "ポップス・アニソン") return L"J-Pop / Anime";
    if (stripped == "EDM / クラブミュージック") return L"EDM / Club Music";
    if (stripped == "ユーロビート") return L"Eurobeat";
    if (stripped == "フュージョン") return L"Fusion";
    if (stripped == "City Pop・Neo Soul") return L"City Pop / Neo Soul";
    if (stripped == "Jazz・Lo-Fi") return L"Jazz / Lo-Fi";
    if (stripped == "Rock・Punk") return L"Rock / Punk";
    if (stripped == "ユーザー定義") return L"User Custom";
    return ToWString(stripped);
}

inline std::wstring GetLocalizedTemplateName(const ProgressionTemplate& t, bool isEn) {
    if (!isEn || t.isUserCustom) return ToWString(t.name);
    if (t.id == "oudo") return L"Royal Road (4536)";
    if (t.id == "marusa") return L"Just The Two of Us Loop";
    if (t.id == "komuro") return L"Komuro Progression (6451)";
    if (t.id == "canon") return L"Canon Progression";
    if (t.id == "pop-punk-four") return L"4-Chord Pop (Let It Be)";
    if (t.id == "cycle") return L"Turnaround (1625)";
    if (t.id == "confirmation-anime") return L"Confirmation Cadence";
    if (t.id == "jpop-emotional") return L"Youth Emotional Chorus";
    if (t.id == "subdominant-minor") return L"Subdominant Minor Cadence";
    if (t.id == "jrock-drive") return L"Fast J-Rock Drive";
    if (t.id == "anison-climax") return L"Anime Climax Chorus";
    if (t.id == "future-bass-kawaii") return L"Future Bass 7th Loop";
    if (t.id == "future-bass-emotional") return L"Future Bass Emotional";
    if (t.id == "progressive-house-anthem") return L"Festival Anthem (Avicii Style)";
    if (t.id == "progressive-house-buildup") return L"Progressive Buildup";
    if (t.id == "melodic-dubstep-illenium") return L"Melodic Dubstep (Illenium Style)";
    if (t.id == "deep-house-minimal") return L"Deep House Minor";
    if (t.id == "deep-house-classic") return L"90s Club Groove";
    if (t.id == "synthwave-retro") return L"80s Synthwave Riff";
    if (t.id == "synthwave-outrun") return L"Outrun Night Drive";
    if (t.id == "eurobeat-initial-d") return L"Initial D Classic Eurobeat";
    if (t.id == "eurobeat-hyper") return L"Hyper Eurobeat";
    if (t.id == "eurobeat-dejavu") return L"Melodic Euro (Deja Vu Cadence)";
    if (t.id == "eurobeat-parapara") return L"90s ParaPara (Night of Fire)";
    if (t.id == "fusion-t-square") return L"T-SQUARE Style Fusion";
    if (t.id == "fusion-casiopea") return L"CASIOPEA Style Slap Funk";
    if (t.id == "fusion-modern") return L"Modern Fusion Tension";
    if (t.id == "fusion-mellow-cruising") return L"Mellow Fusion Ballad";
    if (t.id == "citypop-breeze") return L"80s City Pop Loop";
    if (t.id == "neosoul-groove") return L"Neo Soul Smooth (2-5-1-6)";
    if (t.id == "backdoor-prog") return L"Backdoor Cadence (bVII7-I)";
    if (t.id == "jazz-two-five-one") return L"Major II-V-I";
    if (t.id == "minor-two-five-one") return L"Minor ii-V-i";
    if (t.id == "autumn-cadence") return L"Autumn Leaves Progression";
    if (t.id == "greenday-punk") return L"Punk Rock Anthem";
    if (t.id == "grunge-loop") return L"Grunge / Alternative";
    if (t.id == "hardrock-riff") return L"Hard Rock Minor Riff";
    return ToWString(t.name);
}

inline std::wstring GetLocalizedTemplateDesc(const ProgressionTemplate& t, bool isEn) {
    if (!isEn || t.isUserCustom) return ToWString(t.description);
    if (t.id == "oudo") return L"Most beloved J-Pop & Anime hit progression";
    if (t.id == "marusa") return L"Groovy Neo-Soul & R&B loop with III7 secondary dominant";
    if (t.id == "komuro") return L"Dramatic and driving 90s-modern hit progression";
    if (t.id == "canon") return L"Timeless emotional progression from Pachelbel's Canon";
    if (t.id == "pop-punk-four") return L"World's most famous 4-chord hit progression";
    if (t.id == "cycle") return L"Classic jazz & standard turnaround loop";
    if (t.id == "confirmation-anime") return L"Jazz-inspired emotional anime chorus cadence";
    if (t.id == "jpop-emotional") return L"Bright yet nostalgic J-Pop chorus progression";
    if (t.id == "subdominant-minor") return L"Bittersweet cadence using borrowed minor iv chord";
    if (t.id == "jrock-drive") return L"Upbeat minor progression ideal for intros and verses";
    if (t.id == "anison-climax") return L"Dramatic climax progression resolving strongly to I";
    if (t.id == "future-bass-kawaii") return L"Lush 7th voicings ideal for sidechain synth chords";
    if (t.id == "future-bass-emotional") return L"Uplifting wide synth stab progression";
    if (t.id == "progressive-house-anthem") return L"Evergreen mainstage festival singalong chords";
    if (t.id == "progressive-house-buildup") return L"Rising tension buildup leading into the drop";
    if (t.id == "melodic-dubstep-illenium") return L"Emotional piano & supersaw bass music progression";
    if (t.id == "deep-house-minimal") return L"Hypnotic minimal minor club loop";
    if (t.id == "deep-house-classic") return L"Soulful 90s house progression for organ & stabs";
    if (t.id == "synthwave-retro") return L"Retro-future progression for analog synth arps";
    if (t.id == "synthwave-outrun") return L"Cyberpunk night highway driving progression";
    if (t.id == "eurobeat-initial-d") return L"Explosive speed and melancholy of Super Eurobeat";
    if (t.id == "eurobeat-hyper") return L"High-energy Eurobeat chorus loop";
    if (t.id == "eurobeat-dejavu") return L"Dramatic III7 turnaround pulling back into the loop";
    if (t.id == "eurobeat-parapara") return L"Non-stop 90s dancefloor Eurobeat foundation";
    if (t.id == "fusion-t-square") return L"Refreshing 80s-90s Japanese instrumental fusion";
    if (t.id == "fusion-casiopea") return L"Tight 16-beat slap bass funk progression";
    if (t.id == "fusion-modern") return L"Sophisticated voicings with floating melody sense";
    if (t.id == "fusion-mellow-cruising") return L"Urban night cruising adult fusion ballad";
    if (t.id == "citypop-breeze") return L"Breezy urban tension chord progression";
    if (t.id == "neosoul-groove") return L"Smooth R&B / Neo-Soul 2-5-1-6 turnaround";
    if (t.id == "backdoor-prog") return L"Stylish resolution sliding from bVII7 to IMaj7";
    if (t.id == "jazz-two-five-one") return L"Essential cadence underlying all popular music";
    if (t.id == "minor-two-five-one") return L"Sophisticated minor cadence great for Lo-Fi HipHop";
    if (t.id == "autumn-cadence") return L"Iconic jazz standard circle-of-fifths progression";
    if (t.id == "greenday-punk") return L"Straightforward driving power-chord progression";
    if (t.id == "grunge-loop") return L"Raw alternative rock loop featuring bVII";
    if (t.id == "hardrock-riff") return L"Heavy minor riff progression for hard rock";
    return ToWString(t.description);
}

inline std::wstring NormalizePathString(const std::wstring& pathStr) {
    if (pathStr.empty()) return L"";
    std::wstring out;
    out.reserve(pathStr.size());
    bool prevSep = false;
    for (size_t i = 0; i < pathStr.size(); ++i) {
        wchar_t c = pathStr[i];
        if (c == L'\\' || c == L'/') {
            if (!prevSep || (i == 1 && out[0] == L'\\')) {
                out += L'\\';
            }
            prevSep = true;
        } else {
            out += c;
            prevSep = false;
        }
    }
    return out;
}

} // namespace clp
