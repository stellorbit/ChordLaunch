#include "music_engine.hpp"
#include <algorithm>
#include <cctype>
#include <array>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <windows.h>
#include <shlobj.h>

namespace clp {

std::vector<ProgressionTemplate> MusicEngine::s_userTemplates;
bool MusicEngine::s_userTemplatesLoaded = false;

namespace {

const std::array<std::string, 12> kSharpNotes = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

const std::array<std::string, 12> kFlatNotes = {
    "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"
};

void ParseTensionSubstr(std::string_view tStr, std::vector<int8_t>& outTensions) {
    if (tStr.empty()) return;
    auto addUnique = [&](int8_t val) {
        if (std::find(outTensions.begin(), outTensions.end(), val) == outTensions.end()) {
            outTensions.push_back(val);
        }
    };
    size_t pos = 0;
    while (pos < tStr.size()) {
        while (pos < tStr.size() && (tStr[pos] == ',' || tStr[pos] == ' ' || tStr[pos] == '(' || tStr[pos] == ')')) {
            ++pos;
        }
        if (pos >= tStr.size()) break;
        std::string_view rem = tStr.substr(pos);
        if (rem.starts_with("b9") || rem.starts_with("-9")) {
            addUnique(-9);
            pos += 2;
        } else if (rem.starts_with("#9") || rem.starts_with("+9")) {
            addUnique(99);
            pos += 2;
        } else if (rem.starts_with("9")) {
            addUnique(9);
            pos += 1;
        } else if (rem.starts_with("#11") || rem.starts_with("+11")) {
            addUnique(-11);
            pos += 3;
        } else if (rem.starts_with("11")) {
            addUnique(11);
            pos += 2;
        } else if (rem.starts_with("b13") || rem.starts_with("-13")) {
            addUnique(-13);
            pos += 3;
        } else if (rem.starts_with("13")) {
            addUnique(13);
            pos += 2;
        } else {
            ++pos;
        }
    }
}

} // namespace

std::string MusicEngine::QualityToSuffix(ChordQuality quality) {
    switch (quality) {
        case ChordQuality::Major: return "";
        case ChordQuality::Minor: return "m";
        case ChordQuality::Dominant7: return "7";
        case ChordQuality::Major7: return "Maj7";
        case ChordQuality::Minor7: return "m7";
        case ChordQuality::MinorMajor7: return "mM7";
        case ChordQuality::Diminished: return "dim";
        case ChordQuality::Diminished7: return "dim7";
        case ChordQuality::HalfDiminished7: return "m7b5";
        case ChordQuality::Augmented: return "aug";
        case ChordQuality::Augmented7: return "aug7";
        case ChordQuality::Sus2: return "sus2";
        case ChordQuality::Sus4: return "sus4";
        case ChordQuality::Dominant7Sus4: return "7sus4";
        case ChordQuality::Add9: return "add9";
        case ChordQuality::MinorAdd9: return "madd9";
        case ChordQuality::Six: return "6";
        case ChordQuality::MinorSix: return "m6";
        case ChordQuality::Power5: return "5";
        case ChordQuality::Rest: return "";
        default: return "";
    }
}

PitchClass MusicEngine::NoteToPitch(std::string_view noteName) {
    if (noteName.empty()) return PitchClass::Unknown;

    char base = static_cast<char>(std::toupper(static_cast<unsigned char>(noteName[0])));
    int pitchVal = -1;
    switch (base) {
        case 'C': pitchVal = 0; break;
        case 'D': pitchVal = 2; break;
        case 'E': pitchVal = 4; break;
        case 'F': pitchVal = 5; break;
        case 'G': pitchVal = 7; break;
        case 'A': pitchVal = 9; break;
        case 'B': pitchVal = 11; break;
        default: return PitchClass::Unknown;
    }

    for (size_t i = 1; i < noteName.size(); ++i) {
        char acc = noteName[i];
        if (acc == '#' || acc == '+') pitchVal = (pitchVal + 1) % 12;
        else if (acc == 'b' || acc == 'B') pitchVal = (pitchVal + 11) % 12;
        else break;
    }

    return static_cast<PitchClass>(pitchVal);
}

std::string MusicEngine::PitchToNote(PitchClass pitch, bool preferFlat) {
    int idx = static_cast<int>(pitch);
    if (idx < 0 || idx >= 12) return "?";
    return preferFlat ? kFlatNotes[idx] : kSharpNotes[idx];
}

bool MusicEngine::IsFlatKey(std::string_view keyRoot, MusicalMode mode) {
    if (keyRoot == "F" || keyRoot == "Bb" || keyRoot == "Eb" || keyRoot == "Ab" ||
        keyRoot == "Db" || keyRoot == "Gb" || keyRoot == "d" || keyRoot == "g" ||
        keyRoot == "c" || keyRoot == "f" || keyRoot == "bb" || keyRoot == "eb") {
        return true;
    }
    if (mode == MusicalMode::NaturalMinor || mode == MusicalMode::HarmonicMinor || mode == MusicalMode::MelodicMinor) {
        if (keyRoot == "D" || keyRoot == "G" || keyRoot == "C" || keyRoot == "F") return true;
    }
    return false;
}

std::string MusicEngine::PitchToNoteForKey(PitchClass pitch, std::string_view keyRoot, MusicalMode mode) {
    return PitchToNote(pitch, IsFlatKey(keyRoot, mode));
}

std::vector<std::string> MusicEngine::GetCircleOfFifths(bool counterClockwise) {
    static const std::vector<std::string> kCircleCW = {
        "C", "G", "D", "A", "E", "B", "F#", "Db", "Ab", "Eb", "Bb", "F"
    };
    static const std::vector<std::string> kCircleCCW = {
        "C", "F", "Bb", "Eb", "Ab", "Db", "F#", "B", "E", "A", "D", "G"
    };
    return counterClockwise ? kCircleCCW : kCircleCW;
}

std::vector<int> MusicEngine::GetQualityIntervals(ChordQuality quality) {
    switch (quality) {
        case ChordQuality::Major: return {0, 4, 7};
        case ChordQuality::Minor: return {0, 3, 7};
        case ChordQuality::Dominant7: return {0, 4, 7, 10};
        case ChordQuality::Major7: return {0, 4, 7, 11};
        case ChordQuality::Minor7: return {0, 3, 7, 10};
        case ChordQuality::MinorMajor7: return {0, 3, 7, 11};
        case ChordQuality::Diminished: return {0, 3, 6};
        case ChordQuality::Diminished7: return {0, 3, 6, 9};
        case ChordQuality::HalfDiminished7: return {0, 3, 6, 10};
        case ChordQuality::Augmented: return {0, 4, 8};
        case ChordQuality::Augmented7: return {0, 4, 8, 10};
        case ChordQuality::Sus2: return {0, 2, 7};
        case ChordQuality::Sus4: return {0, 5, 7};
        case ChordQuality::Dominant7Sus4: return {0, 5, 7, 10};
        case ChordQuality::Add9: return {0, 4, 7, 14};
        case ChordQuality::MinorAdd9: return {0, 3, 7, 14};
        case ChordQuality::Six: return {0, 4, 7, 9};
        case ChordQuality::MinorSix: return {0, 3, 7, 9};
        case ChordQuality::Power5: return {0, 7};
        default: return {0, 4, 7};
    }
}

ChordToken MusicEngine::ParseChordSymbol(
    std::string_view symbol,
    std::string_view currentKey,
    MusicalMode currentMode
) {
    ChordToken token;
    token.rawText = std::string(symbol);

    if (symbol.empty()) return token;

    if (symbol == "R" || symbol == "r" || symbol == "N.C." || symbol == "NC" || symbol == "rest" || symbol == "-") {
        token.isRest = true;
        token.quality = ChordQuality::Rest;
        token.rootName = "R";
        token.degreeRoman = "R";
        return token;
    }

    std::string_view mainPart = symbol;
    std::string_view bassPart;
    size_t slashPos = symbol.find('/');
    if (slashPos != std::string_view::npos) {
        mainPart = symbol.substr(0, slashPos);
        bassPart = symbol.substr(slashPos + 1);
        token.bassName = std::string(bassPart);
        token.bassPitch = NoteToPitch(bassPart);
    }

    if (mainPart.empty()) return token;

    // 丸括弧テンション表記 例: C7(b9), Fmaj7(9), G7(#9) の分離
    std::string tensionDisplay;
    size_t parenStart = mainPart.find('(');
    if (parenStart != std::string_view::npos) {
        size_t parenEnd = mainPart.find(')', parenStart);
        std::string_view tPart = (parenEnd != std::string_view::npos)
            ? mainPart.substr(parenStart, parenEnd - parenStart + 1)
            : mainPart.substr(parenStart);
        ParseTensionSubstr(tPart, token.tensions);
        tensionDisplay = std::string(tPart);
        mainPart = mainPart.substr(0, parenStart);
    }

    if (mainPart.empty()) return token;

    PitchClass keyP = NoteToPitch(currentKey);
    if (keyP == PitchClass::Unknown) keyP = PitchClass::C;

    // 先頭のフラット/シャープ付きディグリー判定 (例: bVII, bVI, bIII, #IV, #4dim, b7)
    int accidentalOffset = 0;
    size_t degPrefixOffset = 0;
    if (mainPart.size() >= 2 && (mainPart[0] == 'b' || mainPart[0] == '#')) {
        char nextC = mainPart[1];
        char nextLower = static_cast<char>(std::tolower(static_cast<unsigned char>(nextC)));
        if ((nextC >= '1' && nextC <= '7') || nextLower == 'i' || nextLower == 'v') {
            accidentalOffset = (mainPart[0] == '#') ? 1 : -1;
            degPrefixOffset = 1;
        }
    }

    std::string_view degCandidate = mainPart.substr(degPrefixOffset);
    char firstChar = degCandidate[0];
    bool isArabic = (firstChar >= '1' && firstChar <= '7');

    int romanDeg = -1;
    size_t romanLen = 0;
    bool isLowerRoman = false;

    static const std::pair<const char*, int> kRomans[] = {
        {"vii", 6}, {"vi", 5}, {"iv", 3}, {"v", 4}, {"iii", 2}, {"ii", 1}, {"i", 0}
    };
    std::string candLower = std::string(degCandidate);
    std::transform(candLower.begin(), candLower.end(), candLower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    for (const auto& r : kRomans) {
        if (candLower.starts_with(r.first)) {
            romanDeg = r.second;
            romanLen = strlen(r.first);
            isLowerRoman = std::islower(static_cast<unsigned char>(degCandidate[0])) != 0;
            break;
        }
    }

    if (isArabic || romanDeg >= 0) {
        int scaleStep = isArabic ? (firstChar - '1') : romanDeg;
        size_t prefixLen = degPrefixOffset + (isArabic ? 1 : romanLen);
        std::string suffix = std::string(mainPart.substr(prefixLen));
        std::string sLower = suffix;
        std::transform(sLower.begin(), sLower.end(), sLower.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        static const int kMajOffsets[7] = {0, 2, 4, 5, 7, 9, 11};
        static const int kMinOffsets[7] = {0, 2, 3, 5, 7, 8, 10};
        // ローマ数字でフラット付き (bVII, bVI, bIII 等) の場合は長音階基準からのフラットと解釈
        int baseDist = (accidentalOffset != 0)
            ? kMajOffsets[scaleStep]
            : ((currentMode == MusicalMode::NaturalMinor) ? kMinOffsets[scaleStep] : kMajOffsets[scaleStep]);
        int semitoneDist = (baseDist + accidentalOffset + 12) % 12;
        PitchClass chordPitch = static_cast<PitchClass>((static_cast<int>(keyP) + semitoneDist) % 12);

        token.rootPitch = chordPitch;
        bool preferFlat = (accidentalOffset < 0) || IsFlatKey(currentKey, currentMode);
        token.rootName = PitchToNote(chordPitch, preferFlat);

        // 特殊サフィックス展開 (Ver 2026.9.2 / HelpDialog 準拠)
        if (sLower == "mas" || sLower == "maj7" || (sLower == "m7" && suffix == "M7")) {
            token.quality = ChordQuality::Major7;
            token.qualitySuffix = "Maj7";
        } else if (sLower == "svn" || (sLower == "7" && (! (romanDeg >= 0 && isLowerRoman)))) {
            token.quality = ChordQuality::Dominant7;
            token.qualitySuffix = "7";
        } else if (sLower == "mis" || sLower == "m7" || sLower == "min7" || (sLower == "7" && romanDeg >= 0 && isLowerRoman)) {
            token.quality = ChordQuality::Minor7;
            token.qualitySuffix = "m7";
        } else if (sLower == "hdm" || sLower == "m75" || sLower == "m7b5" || sLower == "m7-5") {
            token.quality = ChordQuality::HalfDiminished7;
            token.qualitySuffix = "m7b5";
        } else if (sLower == "dim7") {
            token.quality = ChordQuality::Diminished7;
            token.qualitySuffix = "dim7";
        } else if (sLower == "dim") {
            token.quality = ChordQuality::Diminished;
            token.qualitySuffix = "dim";
        } else if (sLower == "suf" || sLower == "sus4") {
            token.quality = ChordQuality::Sus4;
            token.qualitySuffix = "sus4";
        } else if (sLower == "sut" || sLower == "sus2") {
            token.quality = ChordQuality::Sus2;
            token.qualitySuffix = "sus2";
        } else if (sLower == "adn" || sLower == "add9") {
            token.quality = ChordQuality::Add9;
            token.qualitySuffix = "add9";
        } else if (sLower == "six" || sLower == "6") {
            token.quality = ChordQuality::Six;
            token.qualitySuffix = "6";
        } else if (sLower == "m" || sLower == "min") {
            token.quality = ChordQuality::Minor;
            token.qualitySuffix = "m";
        } else if (romanDeg >= 0 && suffix.empty()) {
            // ローマ数字単体表記 (例: "iv", "iii", "VI", "bVII") は大文字=Major, 小文字=Minor
            token.quality = isLowerRoman ? ChordQuality::Minor : ChordQuality::Major;
            token.qualitySuffix = QualityToSuffix(token.quality);
        } else {
            static const ChordQuality kDefaultQMaj[7] = {
                ChordQuality::Major, ChordQuality::Minor, ChordQuality::Minor,
                ChordQuality::Major, ChordQuality::Major, ChordQuality::Minor, ChordQuality::HalfDiminished7
            };
            static const ChordQuality kDefaultQMin[7] = {
                ChordQuality::Minor, ChordQuality::HalfDiminished7, ChordQuality::Major,
                ChordQuality::Minor, ChordQuality::Minor, ChordQuality::Major, ChordQuality::Major
            };
            token.quality = (currentMode == MusicalMode::NaturalMinor) ? kDefaultQMin[scaleStep] : kDefaultQMaj[scaleStep];
            token.qualitySuffix = QualityToSuffix(token.quality);
        }

        token.qualitySuffix += tensionDisplay;
        token.degreeRoman = CalculateDegree(token.rootPitch, token.quality, keyP, currentMode, token.bassPitch);
        return token;
    }

    size_t rootLen = 1;
    if (mainPart.size() > 1 && (mainPart[1] == '#' || mainPart[1] == 'b' || mainPart[1] == '+')) {
        rootLen = 2;
    }

    token.rootName = std::string(mainPart.substr(0, rootLen));
    token.rootPitch = NoteToPitch(token.rootName);

    std::string descriptor = std::string(mainPart.substr(rootLen));
    token.qualitySuffix = descriptor;

    std::string descLower = descriptor;
    std::transform(descLower.begin(), descLower.end(), descLower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (descriptor == "M7" || descriptor == "Δ7" || descriptor == "^7" || descLower == "maj7" || descLower == "ma7" || descLower == "mas") {
        token.quality = ChordQuality::Major7;
        token.qualitySuffix = "Maj7";
    } else if (descriptor == "m7" || descLower == "min7" || descLower == "-7" || descLower == "mis") {
        token.quality = ChordQuality::Minor7;
        token.qualitySuffix = "m7";
    } else if (descriptor == "mM7" || descLower == "mmaj7") {
        token.quality = ChordQuality::MinorMajor7;
        token.qualitySuffix = "mM7";
    } else if (descriptor == "m" || descLower == "min") {
        token.quality = ChordQuality::Minor;
        token.qualitySuffix = "m";
    } else if (descLower.empty() || descLower == "maj") {
        token.quality = ChordQuality::Major;
        token.qualitySuffix = "";
    } else if (descLower == "7" || descLower == "dom7" || descLower == "svn") {
        token.quality = ChordQuality::Dominant7;
        token.qualitySuffix = "7";
    } else if (descLower == "m7b5" || descLower == "m7-5" || descLower == "ø" || descLower == "halfdim" || descLower == "hdm" || descLower == "m75") {
        token.quality = ChordQuality::HalfDiminished7;
        token.qualitySuffix = "m7b5";
    } else if (descLower == "dim" || descLower == "diminished") {
        token.quality = ChordQuality::Diminished;
        token.qualitySuffix = "dim";
    } else if (descLower == "dim7" || descLower == "o7") {
        token.quality = ChordQuality::Diminished7;
        token.qualitySuffix = "dim7";
    } else if (descLower == "aug" || descLower == "augmented" || descLower == "+") {
        token.quality = ChordQuality::Augmented;
        token.qualitySuffix = "aug";
    } else if (descLower == "aug7" || descLower == "7#5" || descLower == "7+5" || descLower == "+7") {
        token.quality = ChordQuality::Augmented7;
        token.qualitySuffix = "aug7";
    } else if (descLower == "sus4" || descLower == "4" || descLower == "suf") {
        token.quality = ChordQuality::Sus4;
        token.qualitySuffix = "sus4";
    } else if (descLower == "sus2" || descLower == "2" || descLower == "sut") {
        token.quality = ChordQuality::Sus2;
        token.qualitySuffix = "sus2";
    } else if (descLower == "7sus4" || descLower == "7sus") {
        token.quality = ChordQuality::Dominant7Sus4;
        token.qualitySuffix = "7sus4";
    } else if (descLower == "add9" || descLower == "adn") {
        token.quality = ChordQuality::Add9;
        token.qualitySuffix = "add9";
    } else if (descLower == "madd9") {
        token.quality = ChordQuality::MinorAdd9;
        token.qualitySuffix = "madd9";
    } else if (descLower == "6" || descLower == "six") {
        token.quality = ChordQuality::Six;
        token.qualitySuffix = "6";
    } else if (descLower == "m6") {
        token.quality = ChordQuality::MinorSix;
        token.qualitySuffix = "m6";
    } else if (descLower == "5") {
        token.quality = ChordQuality::Power5;
        token.qualitySuffix = "5";
    } else {
        token.quality = ChordQuality::Major;
    }

    token.qualitySuffix += tensionDisplay;
    token.degreeRoman = CalculateDegree(token.rootPitch, token.quality, keyP, currentMode, token.bassPitch);
    return token;
}

std::string MusicEngine::CalculateDegree(
    PitchClass chordRoot,
    ChordQuality quality,
    PitchClass keyRoot,
    [[maybe_unused]] MusicalMode mode,
    PitchClass bassPitch
) {
    if (chordRoot == PitchClass::Unknown || keyRoot == PitchClass::Unknown) {
        return "-";
    }
    if (quality == ChordQuality::Rest) {
        return "R";
    }

    int distance = (static_cast<int>(chordRoot) - static_cast<int>(keyRoot) + 12) % 12;

    static const char* kMajorRomanRoots[12] = {
        "I", "bII", "II", "bIII", "III", "IV", "#IV", "V", "bVI", "VI", "bVII", "VII"
    };

    std::string rootRoman;
    if (distance == 6 && quality == ChordQuality::Dominant7) {
        rootRoman = "bV";
    } else {
        rootRoman = kMajorRomanRoots[distance];
    }

    std::string suffix;
    switch (quality) {
        case ChordQuality::Major: suffix = ""; break;
        case ChordQuality::Minor: suffix = "m"; break;
        case ChordQuality::Dominant7: suffix = "7"; break;
        case ChordQuality::Major7: suffix = "Maj7"; break;
        case ChordQuality::Minor7: suffix = "m7"; break;
        case ChordQuality::MinorMajor7: suffix = "mM7"; break;
        case ChordQuality::Diminished: suffix = "dim"; break;
        case ChordQuality::Diminished7: suffix = "dim7"; break;
        case ChordQuality::HalfDiminished7: suffix = "m7b5"; break;
        case ChordQuality::Augmented: suffix = "aug"; break;
        case ChordQuality::Augmented7: suffix = "aug7"; break;
        case ChordQuality::Sus2: suffix = "sus2"; break;
        case ChordQuality::Sus4: suffix = "sus4"; break;
        case ChordQuality::Dominant7Sus4: suffix = "7sus4"; break;
        case ChordQuality::Add9: suffix = "add9"; break;
        case ChordQuality::MinorAdd9: suffix = "madd9"; break;
        case ChordQuality::Six: suffix = "6"; break;
        case ChordQuality::MinorSix: suffix = "m6"; break;
        case ChordQuality::Power5: suffix = "5"; break;
        default: suffix = ""; break;
    }

    std::string result = rootRoman + suffix;
    if (bassPitch != PitchClass::Unknown && bassPitch != chordRoot) {
        int bassDist = (static_cast<int>(bassPitch) - static_cast<int>(keyRoot) + 12) % 12;
        result += "/" + std::string(kMajorRomanRoots[bassDist]);
    }
    return result;
}

std::vector<ChordToken> MusicEngine::GetDiatonicChords(
    std::string_view keyRoot,
    MusicalMode mode,
    bool with7th
) {
    return GetDiatonicChordsByTab(keyRoot, mode, with7th ? 1 : 0);
}

std::vector<ChordToken> MusicEngine::GetDiatonicChordsByTab(
    std::string_view keyRoot,
    MusicalMode mode,
    int tabIndex
) {
    PitchClass keyP = NoteToPitch(keyRoot);
    if (keyP == PitchClass::Unknown) keyP = PitchClass::C;

    static const int kMajIntervals[7] = {0, 2, 4, 5, 7, 9, 11};
    static const int kMinIntervals[7] = {0, 2, 3, 5, 7, 8, 10};

    static const ChordQuality kMaj3rdQ[7] = {
        ChordQuality::Major, ChordQuality::Minor, ChordQuality::Minor,
        ChordQuality::Major, ChordQuality::Major, ChordQuality::Minor, ChordQuality::Diminished
    };
    static const ChordQuality kMaj7thQ[7] = {
        ChordQuality::Major7, ChordQuality::Minor7, ChordQuality::Minor7,
        ChordQuality::Major7, ChordQuality::Dominant7, ChordQuality::Minor7, ChordQuality::HalfDiminished7
    };
    static const ChordQuality kMin3rdQ[7] = {
        ChordQuality::Minor, ChordQuality::Diminished, ChordQuality::Major,
        ChordQuality::Minor, ChordQuality::Minor, ChordQuality::Major, ChordQuality::Major
    };
    static const ChordQuality kMin7thQ[7] = {
        ChordQuality::Minor7, ChordQuality::HalfDiminished7, ChordQuality::Major7,
        ChordQuality::Minor7, ChordQuality::Minor7, ChordQuality::Major7, ChordQuality::Dominant7
    };
    static const ChordQuality kSus4Q[7] = {
        ChordQuality::Sus4, ChordQuality::Sus4, ChordQuality::Sus4,
        ChordQuality::Sus4, ChordQuality::Sus4, ChordQuality::Sus4, ChordQuality::Sus4
    };
    static const ChordQuality kAdd9Q[7] = {
        ChordQuality::Add9, ChordQuality::Add9, ChordQuality::Add9,
        ChordQuality::Add9, ChordQuality::Add9, ChordQuality::Add9, ChordQuality::Add9
    };

    const int* intervals = (mode == MusicalMode::NaturalMinor) ? kMinIntervals : kMajIntervals;
    const ChordQuality* qualities = nullptr;
    if (tabIndex == 2) {
        qualities = kSus4Q;
    } else if (tabIndex == 3) {
        qualities = kAdd9Q;
    } else if (mode == MusicalMode::NaturalMinor) {
        qualities = (tabIndex == 1) ? kMin7thQ : kMin3rdQ;
    } else {
        qualities = (tabIndex == 1) ? kMaj7thQ : kMaj3rdQ;
    }

    std::vector<ChordToken> chords;
    chords.reserve(7);

    if (tabIndex == 4) {
        // 定番分数コード (Slash / On-Bass Chords) パレット
        struct SlashDef { int rootOffset; ChordQuality q; int bassOffset; };
        static const SlashDef kMajSlash[7] = {
            {0, ChordQuality::Major, 4},  // I/III (C/E)
            {5, ChordQuality::Major, 7},  // IV/V (F/G)
            {7, ChordQuality::Major, 11}, // V/VII (G/B)
            {9, ChordQuality::Minor, 7},  // VIm/V (Am/G)
            {0, ChordQuality::Major, 7},  // I/V (C/G)
            {2, ChordQuality::Minor, 0},  // IIm/I (Dm/C)
            {4, ChordQuality::Minor, 7}   // IIIm/V (Em/G)
        };
        static const SlashDef kMinSlash[7] = {
            {0, ChordQuality::Minor, 3},  // Im/bIII (Cm/Eb)
            {3, ChordQuality::Major, 7},  // bIII/V (Eb/G)
            {5, ChordQuality::Minor, 7},  // IVm/V (Fm/G)
            {7, ChordQuality::Major, 11}, // V/VII (G/B)
            {8, ChordQuality::Major, 10}, // bVI/bVII (Ab/Bb)
            {10, ChordQuality::Major, 2}, // bVII/II (Bb/D)
            {0, ChordQuality::Minor, 10}  // Im/bVII (Cm/Bb)
        };
        const SlashDef* defs = (mode == MusicalMode::NaturalMinor) ? kMinSlash : kMajSlash;
        for (int i = 0; i < 7; ++i) {
            PitchClass cp = static_cast<PitchClass>((static_cast<int>(keyP) + defs[i].rootOffset) % 12);
            PitchClass bp = static_cast<PitchClass>((static_cast<int>(keyP) + defs[i].bassOffset) % 12);
            std::string sym = PitchToNoteForKey(cp, keyRoot, mode) + QualityToSuffix(defs[i].q) + "/" + PitchToNoteForKey(bp, keyRoot, mode);
            chords.push_back(ParseChordSymbol(sym, keyRoot, mode));
        }
        return chords;
    }

    for (int i = 0; i < 7; ++i) {
        PitchClass chordPitch = static_cast<PitchClass>((static_cast<int>(keyP) + intervals[i]) % 12);
        ChordQuality q = qualities[i];

        ChordToken ct;
        ct.rootPitch = chordPitch;
        ct.rootName = PitchToNoteForKey(chordPitch, keyRoot, mode);
        ct.quality = q;
        ct.qualitySuffix = QualityToSuffix(q);
        ct.degreeRoman = CalculateDegree(chordPitch, q, keyP, mode);
        ct.rawText = ct.rootName + ct.qualitySuffix;
        chords.push_back(ct);
    }
    return chords;
}

std::vector<ChordToken> MusicEngine::GetModalInterchangeChords(
    std::string_view keyRoot,
    MusicalMode mode,
    bool with7th
) {
    PitchClass keyP = NoteToPitch(keyRoot);
    if (keyP == PitchClass::Unknown) keyP = PitchClass::C;

    std::vector<ChordToken> chords;
    auto addToken = [&](int semitoneOffset, ChordQuality q) {
        PitchClass cp = static_cast<PitchClass>((static_cast<int>(keyP) + semitoneOffset) % 12);
        ChordToken ct;
        ct.rootPitch = cp;
        ct.rootName = PitchToNote(cp, true);
        ct.quality = q;
        ct.qualitySuffix = QualityToSuffix(q);
        ct.degreeRoman = CalculateDegree(cp, q, keyP, mode);
        ct.rawText = ct.rootName + ct.qualitySuffix;
        chords.push_back(ct);
    };

    if (mode == MusicalMode::Major) {
        if (with7th) {
            addToken(0, ChordQuality::Minor7);          // Im7
            addToken(2, ChordQuality::HalfDiminished7); // IIm7b5
            addToken(3, ChordQuality::Major7);          // bIIIMaj7
            addToken(5, ChordQuality::Minor7);          // IVm7
            addToken(7, ChordQuality::Minor7);          // Vm7
            addToken(8, ChordQuality::Major7);          // bVIMaj7
            addToken(10, ChordQuality::Dominant7);      // bVII7
        } else {
            addToken(0, ChordQuality::Minor);           // Im
            addToken(2, ChordQuality::Diminished);      // IIdim
            addToken(3, ChordQuality::Major);           // bIII
            addToken(5, ChordQuality::Minor);           // IVm
            addToken(7, ChordQuality::Minor);           // Vm
            addToken(8, ChordQuality::Major);           // bVI
            addToken(10, ChordQuality::Major);          // bVII
        }
    } else {
        if (with7th) {
            addToken(0, ChordQuality::Major7);          // IMaj7
            addToken(2, ChordQuality::Minor7);          // IIm7
            addToken(4, ChordQuality::Minor7);          // IIIm7
            addToken(5, ChordQuality::Major7);          // IVMaj7
            addToken(7, ChordQuality::Dominant7);      // V7
            addToken(9, ChordQuality::Minor7);          // VIm7
            addToken(11, ChordQuality::HalfDiminished7);// VIIm7b5
        } else {
            addToken(0, ChordQuality::Major);           // I
            addToken(2, ChordQuality::Minor);           // IIm
            addToken(4, ChordQuality::Minor);           // IIIm
            addToken(5, ChordQuality::Major);           // IV
            addToken(7, ChordQuality::Major);           // V
            addToken(9, ChordQuality::Minor);           // VIm
            addToken(11, ChordQuality::Diminished);     // VIIdim
        }
    }
    return chords;
}

std::vector<ChordToken> MusicEngine::GetSecondaryDominants(
    std::string_view keyRoot,
    MusicalMode mode
) {
    PitchClass keyP = NoteToPitch(keyRoot);
    if (keyP == PitchClass::Unknown) keyP = PitchClass::C;

    std::vector<ChordToken> chords;
    auto addSecDom = [&](int targetDegreeSemitone, const char* label) {
        PitchClass domPitch = static_cast<PitchClass>((static_cast<int>(keyP) + targetDegreeSemitone + 7) % 12);
        ChordToken ct;
        ct.rootPitch = domPitch;
        ct.rootName = PitchToNoteForKey(domPitch, keyRoot, mode);
        ct.quality = ChordQuality::Dominant7;
        ct.qualitySuffix = "7";
        ct.degreeRoman = label;
        ct.rawText = ct.rootName + ct.qualitySuffix;
        chords.push_back(ct);
    };

    addSecDom(2, "V7/ii");
    addSecDom(4, "V7/iii");
    addSecDom(5, "V7/IV");
    addSecDom(7, "V7/V");
    addSecDom(9, "V7/vi");

    return chords;
}

std::vector<ChordToken> MusicEngine::GetTritoneSubstitutions(
    std::string_view keyRoot,
    MusicalMode mode
) {
    (void)mode;
    PitchClass keyP = NoteToPitch(keyRoot);
    if (keyP == PitchClass::Unknown) keyP = PitchClass::C;

    std::vector<ChordToken> chords;
    auto addSubV = [&](int targetDegreeSemitone, const char* label) {
        PitchClass subPitch = static_cast<PitchClass>((static_cast<int>(keyP) + targetDegreeSemitone + 1) % 12);
        ChordToken ct;
        ct.rootPitch = subPitch;
        ct.rootName = PitchToNote(subPitch, true);
        ct.quality = ChordQuality::Dominant7;
        ct.qualitySuffix = "7";
        ct.degreeRoman = label;
        ct.rawText = ct.rootName + ct.qualitySuffix;
        chords.push_back(ct);
    };

    addSubV(0, "SubV7/I");
    addSubV(2, "SubV7/ii");
    addSubV(4, "SubV7/iii");
    addSubV(5, "SubV7/IV");
    addSubV(7, "SubV7/V");
    addSubV(9, "SubV7/vi");

    return chords;
}

std::vector<ChordToken> MusicEngine::GetSuggestions(
    const std::vector<ChordToken>& chords,
    std::string_view keyRoot,
    MusicalMode mode,
    bool with7th
) {
    auto baseDiatonics = GetDiatonicChords(keyRoot, mode, with7th);
    if (baseDiatonics.empty()) return {};

    auto pick = [&](const std::vector<int>& indices) {
        std::vector<ChordToken> res;
        for (int idx : indices) {
            if (idx >= 0 && idx < static_cast<int>(baseDiatonics.size())) {
                res.push_back(baseDiatonics[idx]);
            }
        }
        return res;
    };

    if (chords.empty()) {
        return pick({0, 4, 5, 3}); // I, V, vi, IV
    }

    const auto& last = chords.back();
    std::string deg = last.degreeRoman;

    if (deg.starts_with("V") && !deg.starts_with("VI")) {
        return pick({0, 5, 3, 1}); // I, vi, IV, ii
    }
    if (deg.starts_with("VI") || deg.starts_with("vi")) {
        return pick({3, 4, 0, 1}); // IV, V, I, ii
    }
    if (deg.starts_with("II") || deg.starts_with("ii")) {
        return pick({4, 0, 5, 3}); // V, I, vi, IV
    }
    if (deg.starts_with("IV") || deg.starts_with("iv")) {
        return pick({4, 0, 2, 5}); // V, I, iii, vi
    }

    return pick({4, 5, 3, 1}); // V, vi, IV, ii
}

std::vector<int> MusicEngine::GetChordMidiNotes(
    const ChordToken& chord,
    int baseOctave,
    VoicingStyle voicing,
    int inversion,
    int bassOctaveMode
) {
    if (chord.isRest || chord.rootPitch == PitchClass::Unknown) {
        return {};
    }

    // コード単位のオクターブシフトを反映（全体の基準オクターブに加算）
    int effectiveOctave = std::clamp(baseOctave + chord.octaveOffset, 0, 8);
    int rootMidi = 12 * (effectiveOctave + 1) + static_cast<int>(chord.rootPitch);
    std::vector<int> intervals = GetQualityIntervals(chord.quality);

    std::vector<int> notes;
    notes.reserve(intervals.size() + 4);

    for (int iv : intervals) {
        notes.push_back(rootMidi + iv);
    }

    for (int8_t t : chord.tensions) {
        int tOffset = 0;
        if (t == 9) tOffset = 14;
        else if (t == -9) tOffset = 13;
        else if (t == 10 || t == 99) tOffset = 15; // #9
        else if (t == 11) tOffset = 17;
        else if (t == -11) tOffset = 18;
        else if (t == 13) tOffset = 21;
        else if (t == -13) tOffset = 20;
        if (tOffset > 0) {
            notes.push_back(rootMidi + tOffset);
        }
    }

    // 転回形 (Inversion) 適用
    int activeInv = (chord.inversion > 0) ? chord.inversion : inversion;
    if (activeInv > 0 && !notes.empty()) {
        int invCount = activeInv % static_cast<int>(notes.size());
        for (int i = 0; i < invCount; ++i) {
            notes[i] += 12;
        }
        std::sort(notes.begin(), notes.end());
    }

    // ボイシング適用
    if (voicing == VoicingStyle::Drop2 && notes.size() >= 4) {
        int dropIdx = static_cast<int>(notes.size()) - 2;
        notes[dropIdx] -= 12;
        std::sort(notes.begin(), notes.end());
    } else if (voicing == VoicingStyle::Drop3 && notes.size() >= 4) {
        int dropIdx = static_cast<int>(notes.size()) - 3;
        notes[dropIdx] -= 12;
        std::sort(notes.begin(), notes.end());
    } else if (voicing == VoicingStyle::OpenSpread && notes.size() >= 3) {
        for (size_t i = 1; i < notes.size(); i += 2) {
            notes[i] += 12;
        }
        std::sort(notes.begin(), notes.end());
    }

    // ベース音追加処理 (bassOctaveMode: 0: なし(標準1音), 1: -1 Oct, 2: -2 Oct, 3: 両方)
    PitchClass bp = (chord.bassPitch != PitchClass::Unknown) ? chord.bassPitch : chord.rootPitch;
    if (bp != PitchClass::Unknown) {
        int bassPitchVal = static_cast<int>(bp);
        int baseMidi = 12 * (effectiveOctave) + bassPitchVal; // C3 (48) at baseOctave=4
        if (bassOctaveMode == 0) {
            // オンコード指定時のみベース音を追加、通常時は追加ベースなし
            if (chord.bassPitch != PitchClass::Unknown) {
                notes.insert(notes.begin(), baseMidi);
            }
        } else if (bassOctaveMode == 1) { // -1 Oct (C3)
            notes.insert(notes.begin(), baseMidi);
        } else if (bassOctaveMode == 2) { // -2 Oct (C2)
            notes.insert(notes.begin(), baseMidi - 12);
        } else if (bassOctaveMode == 3) { // Both (-2 Oct & -1 Oct)
            notes.insert(notes.begin(), baseMidi - 12);
            notes.insert(notes.begin() + 1, baseMidi);
        }
    }

    return notes;
}

std::vector<MusicEngine::TemplateInfo> MusicEngine::GetBuiltinTemplates() {
    return {
        {"oudo", "王道進行 (4536)", "| IVMaj7 | V7 | iiim7 | vim7 |", "J-POP・アニソンで最も愛される大ヒット王道進行"},
        {"marusa", "丸サ進行 (Just The Two of Us)", "| IVMaj7 | III7 | vim7 | I7 |", "椎名林檎やNeo Soulで絶大な人気を誇るお洒落ループ"},
        {"komuro", "小室進行", "| vi | IV | V | I |", "ドラマチックで力強い90年代〜現代ヒット進行"},
        {"canon", "カノン進行", "| I | V | vi | iii | IV | I | IV | V |", "誰もが耳にしたことのあるクラシック由来の感動進行"},
        {"pop-punk", "4コード進行 (Let It Be)", "| I | V | vi | IV |", "世界中で最も多くのヒット曲を生んだ4コード"},
        {"two-five-one", "2-5-1 進行 (循環)", "| iim7 | V7 | IMaj7 | IMaj7 |", "ジャズやポップスの基本カデンツ"},
        {"anison-climax", "アニソン大サビ進行", "| IV | V | iiim | vi | ii | V | I | I |", "サビの後半で一気に盛り上げて完結するドラマチック進行"}
    };
}

// ==========================================
// Ver 2026.9.2 準拠の全ジャンル 31+ テンプレート
// ==========================================
const std::vector<ProgressionTemplate>& MusicEngine::GetAllGenreTemplates() {
    LoadUserTemplates();
    static const std::vector<ProgressionTemplate> kDefaultTemplates = {
        // 🇯🇵 ポップス・アニソン
        {"oudo", "🇯🇵 ポップス・アニソン", "🔥 人気・定番", "王道進行 (4536)", "IVMaj7 V7 iiim7 vim7", "J-POP・アニソンで最も愛される大ヒット王道進行", false},
        {"marusa", "🇯🇵 ポップス・アニソン", "🔥 人気・定番", "丸サ進行 (Just The Two of Us)", "IVMaj7 III7 vim7 I7", "椎名林檎やNeo Soulで絶大な人気を誇るお洒落ループ", false},
        {"komuro", "🇯🇵 ポップス・アニソン", "🔥 人気・定番", "小室進行", "vi IV V I", "ドラマチックで力強い90年代〜現代ヒット進行", false},
        {"canon", "🇯🇵 ポップス・アニソン", "🔥 人気・定番", "カノン進行", "I V vi iii IV I IV V", "誰もが耳にしたことのあるクラシック由来の感動進行", false},
        {"pop-punk-four", "🇯🇵 ポップス・アニソン", "🔥 人気・定番", "4コード進行 (Let It Be)", "I V vi IV", "世界中で最も多くのヒット曲を生んだ4コード", false},
        {"cycle", "🇯🇵 ポップス・アニソン", "🔥 人気・定番", "循環進行", "I VI ii V", "ジャズやスタンダードで回り続ける基本ループ", false},
        {"confirmation-anime", "🇯🇵 ポップス・アニソン", "🇯🇵 J-POP・アニソン", "コンファメ進行 (Confirmation)", "IMaj7 viim7b5 III7 vim7 Vm7 I7 IVMaj7", "アニソンやJ-POPで名高いジャズ由来の究極エモーショナル進行", false},
        {"jpop-emotional", "🇯🇵 ポップス・アニソン", "🇯🇵 J-POP・アニソン", "青春エモ進行", "IVMaj7 V7 IMaj7 vi7", "爽やかさと切なさが両立するサビ進行", false},
        {"subdominant-minor", "🇯🇵 ポップス・アニソン", "🇯🇵 J-POP・アニソン", "サブドミナントマイナー終止", "I V IV iv I", "iv (IVm) の借用和音で胸を締めつける切ない終止", false},
        {"jrock-drive", "🇯🇵 ポップス・アニソン", "🇯🇵 J-POP・アニソン", "疾走J-Rockループ", "vi V IV V", "疾走感のあるAメロやイントロに最適なマイナードライブ", false},
        {"anison-climax", "🇯🇵 ポップス・アニソン", "🇯🇵 J-POP・アニソン", "アニソン大サビ進行", "IV V iiim vi ii V I", "サビの後半で一気に盛り上げて完結するドラマチック進行", false},

        // ⚡ EDM / クラブミュージック
        {"future-bass-kawaii", "⚡ EDM / クラブミュージック", "⚡ Future Bass", "Future Bass 7thループ", "IVMaj7 V7 vim7 IMaj7", "豊かなボイシングでサイドチェインをかけると映えるモダンFuture Bass進行", false},
        {"future-bass-emotional", "⚡ EDM / クラブミュージック", "⚡ Future Bass", "Future Bass エモーショナル展開", "IVMaj7 V7 iiim7 vim7 IVMaj7 V7 IMaj7", "広がりのあるシンセスタブで感情を高揚させる展開進行", false},
        {"progressive-house-anthem", "⚡ EDM / クラブミュージック", "⚡ Progressive House", "フェス系アンセム4コード (Avicii Style)", "vi IV I V", "巨大フェスで大合唱を生み出すエバーグリーンなアンセム進行", false},
        {"progressive-house-buildup", "⚡ EDM / クラブミュージック", "⚡ Progressive House", "プログレッシブ・ビルドアップ", "IV V vi I", "ドロップ（サビ）へ向けてエネルギーを極限まで溜め込む進行", false},
        {"melodic-dubstep-illenium", "⚡ EDM / クラブミュージック", "⚡ Melodic Dubstep", "メロディック・ダブステップ (Illenium Style)", "vi IV I V", "美しいピアノとスーパソーが絡み合うエモ系ベースミュージック", false},
        {"deep-house-minimal", "⚡ EDM / クラブミュージック", "⚡ Deep House", "ディープハウス・マイナー", "i v VI VII", "淡々と繰り返されるクラブトラック定番ミニマルループ", false},
        {"deep-house-classic", "⚡ EDM / クラブミュージック", "⚡ Deep House", "90s クラブ・グルーヴ", "im7 ivm7 vm7 im7", "ソウルフルなコードスタブとオルガンベースに最適な90sハウス進行", false},
        {"synthwave-retro", "⚡ EDM / クラブミュージック", "⚡ Synthwave / Retro", "80s シンセウェーブ・リフ", "i bVII bVI bVII", "アナログシンセのアルペジオと相性抜群のレトロフューチャー進行", false},
        {"synthwave-outrun", "⚡ EDM / クラブミュージック", "⚡ Synthwave / Retro", "アウトラン・ドライブ", "i bVI bIII bVII", "夜のハイウェイを疾走するサイバーパンク・レトロウェーブ進行", false},

        // 🏎️ ユーロビート
        {"eurobeat-initial-d", "🏎️ ユーロビート", "🏎️ ユーロビート", "哀愁ユーロ進行 (Initial D Classic)", "vi IV V I", "頭文字D・スーパーユーロビートの代名詞。疾走感と切なさが爆発する王道マイナーループ", false},
        {"eurobeat-hyper", "🏎️ ユーロビート", "🏎️ ユーロビート", "ハイパー・ユーロビート", "IV V vi vi", "パラパラやハイパーユーロのサビで畳み掛ける強烈な高揚感", false},
        {"eurobeat-dejavu", "🏎️ ユーロビート", "🏎️ ユーロビート", "哀愁メロディック・ユーロ (Deja Vu Cadence)", "vi IV V III7", "サビの終わりにIII7で緊張感を高め次のループへと引き戻すドラマチック進行", false},
        {"eurobeat-parapara", "🏎️ ユーロビート", "🏎️ ユーロビート", "90s パラパラ進行 (Night of Fire Loop)", "vi V IV V", "90年代ダンスフロアを熱狂させたノンストップ・ユーロビートの基本形", false},

        // 🎷 フュージョン
        {"fusion-t-square", "🎷 フュージョン", "🎷 フュージョン", "T-SQUARE風 爽快フュージョン", "IVMaj7 V iiim7 VI7 iim7 V7 IMaj7", "80〜90年代の日本のインスト・フュージョン黄金期を象徴する爽快進行", false},
        {"fusion-casiopea", "🎷 フュージョン", "🎷 フュージョン", "CASIOPEA風 スラップ・ファンク", "iim7 V7 iiim7 VI7 iim7 V7 IMaj7", "タイトな16ビートスラップベースと相性抜群のキレ味鋭いファンキー進行", false},
        {"fusion-modern", "🎷 フュージョン", "🎷 フュージョン", "Modern Fusion テンション進行", "IVMaj7 V7 iiim7 vim7 iim7 V7 IMaj7", "洗練されたボイシングとメロディの浮遊感を引き出すモダン進行", false},
        {"fusion-mellow-cruising", "🎷 フュージョン", "🎷 フュージョン", "メロウ・フュージョン・バラード", "IVMaj7 III7 vim7 Vm7 I7 IVMaj7 V7 IMaj7", "アーバンな夜のドライブを想起させる大人のフュージョンバラード", false},

        // 🌃 City Pop・Neo Soul
        {"citypop-breeze", "🌃 City Pop・Neo Soul", "🌃 City Pop・Neo Soul", "80s シティポップ・ループ", "IVMaj7 iii7 vim7 ii7 V7", "都会的で浮遊感のあるテンションコード進行", false},
        {"neosoul-groove", "🌃 City Pop・Neo Soul", "🌃 City Pop・Neo Soul", "ネオソウル・スムーズ", "iim7 V7 IMaj7 VI7", "グルーヴ感あふれるR&B/ソウルの定番2-5-1-6", false},
        {"backdoor-prog", "🌃 City Pop・Neo Soul", "🌃 City Pop・Neo Soul", "バックドア終止進行", "IMaj7 IVMaj7 bVII7 IMaj7", "bVII7からIへ滑り込むお洒落な解決", false},

        // ☕ Jazz・Lo-Fi
        {"jazz-two-five-one", "☕ Jazz・Lo-Fi", "☕ Jazz・Lo-Fi", "メジャー II-V-I", "iim7 V7 IMaj7", "すべてのポピュラー音楽の基盤となる重要カデンツ", false},
        {"minor-two-five-one", "☕ Jazz・Lo-Fi", "☕ Jazz・Lo-Fi", "マイナー II-V-I", "iim7b5 V7 im7", "ビターで大人びたLo-Fi HipHopに最適なマイナー進行", false},
        {"autumn-cadence", "☕ Jazz・Lo-Fi", "☕ Jazz・Lo-Fi", "枯葉 (Autumn Leaves)", "iim7 V7 IMaj7 IVMaj7 viim7b5 III7 vim", "ジャズスタンダードの代名詞的な美しい進行", false},

        // 🎸 Rock・Punk
        {"greenday-punk", "🎸 Rock・Punk", "🎸 Rock・Punk", "パンクロック定番", "I V vi IV", "パワーコードでストレートに突っ走る直球進行", false},
        {"grunge-loop", "🎸 Rock・Punk", "🎸 Rock・Punk", "グランジ・オルタナティブ", "I bVII IV I", "bVIIのラフな歪み感が心地よいロックループ", false},
        {"hardrock-riff", "🎸 Rock・Punk", "🎸 Rock・Punk", "ハードロック・リフ進行", "i bVII bVI bVII", "ヘビーなリフと相性抜群のマイナーリフ進行", false}
    };

    static std::vector<ProgressionTemplate> s_combined;
    s_combined = kDefaultTemplates;
    for (const auto& ut : s_userTemplates) {
        s_combined.push_back(ut);
    }
    return s_combined;
}

std::vector<std::string> MusicEngine::GetTemplateMajorCategories() {
    return {
        "🇯🇵 ポップス・アニソン",
        "⚡ EDM / クラブミュージック",
        "🏎️ ユーロビート",
        "🎷 フュージョン",
        "🌃 City Pop・Neo Soul",
        "☕ Jazz・Lo-Fi",
        "🎸 Rock・Punk",
        "👤 ユーザー定義"
    };
}

std::vector<ProgressionTemplate> MusicEngine::GetTemplatesByCategory(std::string_view category) {
    const auto& all = GetAllGenreTemplates();
    std::vector<ProgressionTemplate> filtered;
    for (const auto& t : all) {
        if (t.majorCategory == category) {
            filtered.push_back(t);
        }
    }
    return filtered;
}

void MusicEngine::LoadUserTemplates() {
    if (s_userTemplatesLoaded) return;
    s_userTemplatesLoaded = true;
    s_userTemplates.clear();

    wchar_t appDataPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, appDataPath))) {
        std::filesystem::path dir = std::filesystem::path(appDataPath) / L"ChordLaunch";
        std::filesystem::path file = dir / L"user_templates.txt";
        if (std::filesystem::exists(file)) {
            std::ifstream ifs(file);
            std::string line;
            while (std::getline(ifs, line)) {
                if (line.empty() || line[0] == '#') continue;
                std::stringstream ss(line);
                std::string id, name, prog, desc;
                if (std::getline(ss, id, '\t') &&
                    std::getline(ss, name, '\t') &&
                    std::getline(ss, prog, '\t') &&
                    std::getline(ss, desc, '\t')) {
                    ProgressionTemplate pt;
                    pt.id = id;
                    pt.majorCategory = "👤 ユーザー定義";
                    pt.subCategory = "カスタム";
                    pt.name = name;
                    pt.progression = prog;
                    pt.description = desc;
                    pt.isUserCustom = true;
                    s_userTemplates.push_back(pt);
                }
            }
        }
    }
}

bool MusicEngine::SaveUserTemplate(const ProgressionTemplate& tmpl) {
    LoadUserTemplates();
    s_userTemplates.erase(
        std::remove_if(s_userTemplates.begin(), s_userTemplates.end(), [&](const ProgressionTemplate& t) {
            return t.id == tmpl.id;
        }),
        s_userTemplates.end()
    );

    ProgressionTemplate copy = tmpl;
    copy.majorCategory = "👤 ユーザー定義";
    copy.isUserCustom = true;
    s_userTemplates.push_back(copy);

    wchar_t appDataPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, appDataPath))) {
        std::filesystem::path dir = std::filesystem::path(appDataPath) / L"ChordLaunch";
        std::filesystem::create_directories(dir);
        std::filesystem::path file = dir / L"user_templates.txt";
        std::ofstream ofs(file);
        if (ofs.is_open()) {
            for (const auto& t : s_userTemplates) {
                ofs << t.id << "\t" << t.name << "\t" << t.progression << "\t" << t.description << "\n";
            }
            return true;
        }
    }
    return false;
}

bool MusicEngine::DeleteUserTemplate(std::string_view id) {
    LoadUserTemplates();
    size_t prevSize = s_userTemplates.size();
    s_userTemplates.erase(
        std::remove_if(s_userTemplates.begin(), s_userTemplates.end(), [&](const ProgressionTemplate& t) {
            return t.id == id;
        }),
        s_userTemplates.end()
    );

    if (s_userTemplates.size() == prevSize) return false;

    wchar_t appDataPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, appDataPath))) {
        std::filesystem::path dir = std::filesystem::path(appDataPath) / L"ChordLaunch";
        std::filesystem::path file = dir / L"user_templates.txt";
        std::ofstream ofs(file);
        if (ofs.is_open()) {
            for (const auto& t : s_userTemplates) {
                ofs << t.id << "\t" << t.name << "\t" << t.progression << "\t" << t.description << "\n";
            }
            return true;
        }
    }
    return false;
}

} // namespace clp
