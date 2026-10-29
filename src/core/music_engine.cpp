#include "music_engine.hpp"
#include <algorithm>
#include <cctype>
#include <array>
#include <sstream>

namespace clp {

namespace {

const std::array<std::string, 12> kSharpNotes = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

const std::array<std::string, 12> kFlatNotes = {
    "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"
};

} // namespace

PitchClass MusicEngine::NoteToPitch(std::string_view noteName) {
    if (noteName.empty()) return PitchClass::Unknown;

    char base = static_cast<char>(std::toupper(noteName[0]));
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
    // F, Bb, Eb, Ab, Db, Gb やマイナーキーでのフラット調判定
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
        case ChordQuality::Rest: return {};
        default: return {0, 4, 7};
    }
}

ChordToken MusicEngine::ParseChordSymbol(std::string_view symbol, std::string_view currentKey, MusicalMode currentMode) {
    ChordToken token;
    token.rawText = std::string(symbol);

    if (symbol.empty()) return token;

    // 休符 (Rest / N.C.) 判定
    if (symbol == "R" || symbol == "r" || symbol == "N.C." || symbol == "NC" || symbol == "rest" || symbol == "-") {
        token.isRest = true;
        token.quality = ChordQuality::Rest;
        token.rootName = "R";
        token.degreeRoman = "R";
        return token;
    }

    // 分数コード / オンコード (/Bass) の分離
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

    // ルート音名の抽出 (1文字目 + オプションで '#' または 'b')
    size_t rootLen = 1;
    if (mainPart.size() > 1 && (mainPart[1] == '#' || mainPart[1] == 'b' || mainPart[1] == '+')) {
        rootLen = 2;
    }

    token.rootName = std::string(mainPart.substr(0, rootLen));
    token.rootPitch = NoteToPitch(token.rootName);

    std::string descriptor = std::string(mainPart.substr(rootLen));
    token.qualitySuffix = descriptor;

    // クオリティ判定
    // 小文字化して判定
    std::string descLower = descriptor;
    std::transform(descLower.begin(), descLower.end(), descLower.begin(), [](unsigned char c) { return (char)std::tolower(c); });

    if (descLower.empty() || descLower == "maj" || descLower == "m") {
        if (descriptor == "m") {
            token.quality = ChordQuality::Minor;
        } else {
            token.quality = ChordQuality::Major;
        }
    } else if (descLower == "m7" || descLower == "min7" || descLower == "-7") {
        token.quality = ChordQuality::Minor7;
    } else if (descLower == "maj7" || descLower == "m7" || descLower == "delta7" || descLower == "^7" || descLower == "ma7") {
        token.quality = ChordQuality::Major7;
    } else if (descLower == "7" || descLower == "dom7") {
        token.quality = ChordQuality::Dominant7;
    } else if (descLower == "m7b5" || descLower == "m7-5" || descLower == "ø" || descLower == "halfdim") {
        token.quality = ChordQuality::HalfDiminished7;
    } else if (descLower == "dim" || descLower == "diminished") {
        token.quality = ChordQuality::Diminished;
    } else if (descLower == "dim7" || descLower == "o7") {
        token.quality = ChordQuality::Diminished7;
    } else if (descLower == "aug" || descLower == "augmented" || descLower == "+") {
        token.quality = ChordQuality::Augmented;
    } else if (descLower == "aug7" || descLower == "7#5" || descLower == "7+5" || descLower == "+7") {
        token.quality = ChordQuality::Augmented7;
    } else if (descLower == "sus4" || descLower == "4") {
        token.quality = ChordQuality::Sus4;
    } else if (descLower == "7sus4" || descLower == "7sus") {
        token.quality = ChordQuality::Dominant7Sus4;
    } else if (descLower == "sus2" || descLower == "2") {
        token.quality = ChordQuality::Sus2;
    } else if (descLower == "add9" || descLower == "add2") {
        token.quality = ChordQuality::Add9;
    } else if (descLower == "madd9") {
        token.quality = ChordQuality::MinorAdd9;
    } else if (descLower == "6") {
        token.quality = ChordQuality::Six;
    } else if (descLower == "m6") {
        token.quality = ChordQuality::MinorSix;
    } else if (descLower == "5") {
        token.quality = ChordQuality::Power5;
    } else if (descLower == "mmaj7" || descLower == "mm7" || descLower == "m(maj7)") {
        token.quality = ChordQuality::MinorMajor7;
    } else {
        // 部分一致フォールバック
        if (descLower.find("m7b5") != std::string::npos || descLower.find("m7-5") != std::string::npos) {
            token.quality = ChordQuality::HalfDiminished7;
        } else if (descLower.find("maj7") != std::string::npos || descLower.find("m7") != std::string::npos) {
            token.quality = ChordQuality::Major7;
        } else if (descLower.starts_with("m") && descLower != "maj" && !descLower.starts_with("maj")) {
            token.quality = ChordQuality::Minor;
        } else if (descLower.find("7") != std::string::npos) {
            token.quality = ChordQuality::Dominant7;
        } else {
            token.quality = ChordQuality::Major;
        }
    }

    // ディグリーの決定論的計算（"?" を一切返さない）
    PitchClass keyP = NoteToPitch(currentKey);
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

    // 12半音すべてのルート・ディグリー定義（Major基準）
    // 0: I, 1: bII, 2: II, 3: bIII, 4: III, 5: IV, 6: #IV, 7: V, 8: bVI, 9: VI, 10: bVII, 11: VII
    static const char* kMajorRomanRoots[12] = {
        "I", "bII", "II", "bIII", "III", "IV", "#IV", "V", "bVI", "VI", "bVII", "VII"
    };

    // Minor (Aeolian) 基準
    static const char* kMinorRomanRoots[12] = {
        "I", "bII", "II", "bIII", "III", "IV", "#IV", "V", "bVI", "VI", "bVII", "VII"
    };

    std::string rootRoman;
    if (distance == 6 && quality == ChordQuality::Dominant7) {
        // 裏コード（Tritone Sub）時は bV7 と表記
        rootRoman = "bV";
    } else {
        rootRoman = kMajorRomanRoots[distance];
    }

    // クオリティ装飾
    std::string suffix;
    switch (quality) {
        case ChordQuality::Major:
            suffix = "";
            break;
        case ChordQuality::Minor:
            suffix = "m";
            break;
        case ChordQuality::Dominant7:
            suffix = "7";
            break;
        case ChordQuality::Major7:
            suffix = "maj7";
            break;
        case ChordQuality::Minor7:
            suffix = "m7";
            break;
        case ChordQuality::MinorMajor7:
            suffix = "mM7";
            break;
        case ChordQuality::Diminished:
            suffix = "dim";
            break;
        case ChordQuality::Diminished7:
            suffix = "dim7";
            break;
        case ChordQuality::HalfDiminished7:
            suffix = "m7b5";
            break;
        case ChordQuality::Augmented:
            suffix = "aug";
            break;
        case ChordQuality::Augmented7:
            suffix = "aug7";
            break;
        case ChordQuality::Sus2:
            suffix = "sus2";
            break;
        case ChordQuality::Sus4:
            suffix = "sus4";
            break;
        case ChordQuality::Dominant7Sus4:
            suffix = "7sus4";
            break;
        case ChordQuality::Add9:
            suffix = "add9";
            break;
        case ChordQuality::MinorAdd9:
            suffix = "madd9";
            break;
        case ChordQuality::Six:
            suffix = "6";
            break;
        case ChordQuality::MinorSix:
            suffix = "m6";
            break;
        case ChordQuality::Power5:
            suffix = "5";
            break;
        default:
            break;
    }

    std::string result = rootRoman + suffix;

    // オンベース装飾
    if (bassPitch != PitchClass::Unknown && bassPitch != chordRoot) {
        int bassDist = (static_cast<int>(bassPitch) - static_cast<int>(keyRoot) + 12) % 12;
        result += "/" + std::string(kMajorRomanRoots[bassDist]);
    }

    return result;
}

std::vector<ChordToken> MusicEngine::GetDiatonicChords(std::string_view keyRoot, MusicalMode mode, bool with7th) {
    PitchClass keyP = NoteToPitch(keyRoot);
    if (keyP == PitchClass::Unknown) keyP = PitchClass::C;

    // メジャースケール: 0, 2, 4, 5, 7, 9, 11
    // ダイアトニックトライアド: Maj, min, min, Maj, Maj, min, dim
    // ダイアトニック7th: Maj7, m7, m7, Maj7, 7, m7, m7b5
    static const int kMajorScaleSteps[7] = {0, 2, 4, 5, 7, 9, 11};
    static const ChordQuality kTriadsMajor[7] = {
        ChordQuality::Major, ChordQuality::Minor, ChordQuality::Minor,
        ChordQuality::Major, ChordQuality::Major, ChordQuality::Minor, ChordQuality::Diminished
    };
    static const ChordQuality k7thsMajor[7] = {
        ChordQuality::Major7, ChordQuality::Minor7, ChordQuality::Minor7,
        ChordQuality::Major7, ChordQuality::Dominant7, ChordQuality::Minor7, ChordQuality::HalfDiminished7
    };

    // ナチュラルマイナースケール: 0, 2, 3, 5, 7, 8, 10
    static const int kMinorScaleSteps[7] = {0, 2, 3, 5, 7, 8, 10};
    static const ChordQuality kTriadsMinor[7] = {
        ChordQuality::Minor, ChordQuality::Diminished, ChordQuality::Major,
        ChordQuality::Minor, ChordQuality::Minor, ChordQuality::Major, ChordQuality::Major
    };
    static const ChordQuality k7thsMinor[7] = {
        ChordQuality::Minor7, ChordQuality::HalfDiminished7, ChordQuality::Major7,
        ChordQuality::Minor7, ChordQuality::Minor7, ChordQuality::Major7, ChordQuality::Dominant7
    };

    const int* steps = (mode == MusicalMode::NaturalMinor) ? kMinorScaleSteps : kMajorScaleSteps;
    const ChordQuality* qualities = with7th ?
        ((mode == MusicalMode::NaturalMinor) ? k7thsMinor : k7thsMajor) :
        ((mode == MusicalMode::NaturalMinor) ? kTriadsMinor : kTriadsMajor);

    std::vector<ChordToken> result;
    result.reserve(7);

    for (int i = 0; i < 7; ++i) {
        PitchClass p = static_cast<PitchClass>((static_cast<int>(keyP) + steps[i]) % 12);
        ChordToken t;
        t.rootPitch = p;
        t.quality = qualities[i];
        t.rootName = PitchToNoteForKey(p, keyRoot, mode);
        t.degreeRoman = CalculateDegree(t.rootPitch, t.quality, keyP, mode);
        result.push_back(std::move(t));
    }

    return result;
}

std::vector<ChordToken> MusicEngine::GetModalInterchangeChords(std::string_view keyRoot, MusicalMode mode) {
    // 同主調（モーダルインターチェンジ）借用和音
    PitchClass keyP = NoteToPitch(keyRoot);
    if (keyP == PitchClass::Unknown) keyP = PitchClass::C;

    std::vector<ChordToken> result;

    if (mode == MusicalMode::Major) {
        // 同主短調（Parallel Minor）からの代表的借用和音:
        // Im7, IIm7b5, bIIImaj7, IVm7, Vm7, bVImaj7, bVII7
        struct BorrowedDef { int semitones; ChordQuality q; };
        static const BorrowedDef kParallelMinor[7] = {
            {0, ChordQuality::Minor7},
            {2, ChordQuality::HalfDiminished7},
            {3, ChordQuality::Major7},
            {5, ChordQuality::Minor7},
            {7, ChordQuality::Minor7},
            {8, ChordQuality::Major7},
            {10, ChordQuality::Dominant7}
        };
        for (const auto& item : kParallelMinor) {
            PitchClass p = static_cast<PitchClass>((static_cast<int>(keyP) + item.semitones) % 12);
            ChordToken t;
            t.rootPitch = p;
            t.quality = item.q;
            t.rootName = PitchToNote(p, true); // 借用はフラット優先
            t.degreeRoman = CalculateDegree(t.rootPitch, t.quality, keyP, mode);
            result.push_back(std::move(t));
        }
    } else {
        // 同主長調（Parallel Major）からの代表的借用和音:
        // Imaj7, IIm7, IIIm7, IVmaj7, V7, VIm7, VIIm7b5
        struct BorrowedDef { int semitones; ChordQuality q; };
        static const BorrowedDef kParallelMajor[7] = {
            {0, ChordQuality::Major7},
            {2, ChordQuality::Minor7},
            {4, ChordQuality::Minor7},
            {5, ChordQuality::Major7},
            {7, ChordQuality::Dominant7},
            {9, ChordQuality::Minor7},
            {11, ChordQuality::HalfDiminished7}
        };
        for (const auto& item : kParallelMajor) {
            PitchClass p = static_cast<PitchClass>((static_cast<int>(keyP) + item.semitones) % 12);
            ChordToken t;
            t.rootPitch = p;
            t.quality = item.q;
            t.rootName = PitchToNoteForKey(p, keyRoot, mode);
            t.degreeRoman = CalculateDegree(t.rootPitch, t.quality, keyP, mode);
            result.push_back(std::move(t));
        }
    }

    return result;
}

std::vector<ChordToken> MusicEngine::GetSecondaryDominants(std::string_view keyRoot, MusicalMode mode) {
    PitchClass keyP = NoteToPitch(keyRoot);
    if (keyP == PitchClass::Unknown) keyP = PitchClass::C;

    // V7/ii, V7/iii, V7/IV, V7/V, V7/vi
    // それぞれ解決先ルートの完全5度上 (+7半音 = -5半音)
    // ii: +2 -> 5度上は 9 (VI7 = V7/ii)
    // iii: +4 -> 5度上は 11 (VII7 = V7/iii)
    // IV: +5 -> 5度上は 0 (I7 = V7/IV)
    // V: +7 -> 5度上は 2 (II7 = V7/V)
    // vi: +9 -> 5度上は 4 (III7 = V7/vi)
    struct SecDomDef { int semitone; const char* targetDegree; };
    static const SecDomDef kSecDefs[5] = {
        {9, "V7/ii"},
        {11, "V7/iii"},
        {0, "V7/IV"},
        {2, "V7/V"},
        {4, "V7/vi"}
    };

    std::vector<ChordToken> result;
    for (const auto& def : kSecDefs) {
        PitchClass p = static_cast<PitchClass>((static_cast<int>(keyP) + def.semitone) % 12);
        ChordToken t;
        t.rootPitch = p;
        t.quality = ChordQuality::Dominant7;
        t.rootName = PitchToNoteForKey(p, keyRoot, mode);
        t.degreeRoman = def.targetDegree;
        result.push_back(std::move(t));
    }
    return result;
}

std::vector<ChordToken> MusicEngine::GetTritoneSubstitutions(std::string_view keyRoot, [[maybe_unused]] MusicalMode mode) {
    PitchClass keyP = NoteToPitch(keyRoot);
    if (keyP == PitchClass::Unknown) keyP = PitchClass::C;

    // 主要裏コード (SubV7)
    // SubV7/I (bII7: +1半音)
    // SubV7/ii (bVI7: +8半音)
    // SubV7/IV (bVII7: +10半音)
    // SubV7/V (bIII7: +3半音)
    struct SubVDef { int semitone; const char* deg; };
    static const SubVDef kSubs[4] = {
        {1, "bII7 (SubV7/I)"},
        {3, "bIII7 (SubV7/V)"},
        {8, "bVI7 (SubV7/ii)"},
        {10, "bVII7 (SubV7/IV)"}
    };

    std::vector<ChordToken> result;
    for (const auto& s : kSubs) {
        PitchClass p = static_cast<PitchClass>((static_cast<int>(keyP) + s.semitone) % 12);
        ChordToken t;
        t.rootPitch = p;
        t.quality = ChordQuality::Dominant7;
        t.rootName = PitchToNote(p, true);
        t.degreeRoman = s.deg;
        result.push_back(std::move(t));
    }
    return result;
}

std::vector<int> MusicEngine::GetChordMidiNotes(
    const ChordToken& chord,
    int baseOctave,
    VoicingStyle voicing
) {
    if (chord.isRest || chord.rootPitch == PitchClass::Unknown) {
        return {};
    }

    int rootMidi = 12 * (baseOctave + 1) + static_cast<int>(chord.rootPitch);
    std::vector<int> intervals = GetQualityIntervals(chord.quality);

    std::vector<int> notes;
    notes.reserve(intervals.size() + 2);

    for (int iv : intervals) {
        notes.push_back(rootMidi + iv);
    }

    // テンション追加
    for (int8_t t : chord.tensions) {
        int tOffset = 0;
        if (t == 9) tOffset = 14;
        else if (t == -9) tOffset = 13; // b9
        else if (t == 10) tOffset = 15; // #9
        else if (t == 11) tOffset = 17; // 11
        else if (t == -11) tOffset = 18; // #11
        else if (t == 13) tOffset = 21; // 13
        else if (t == -13) tOffset = 20; // b13
        if (tOffset > 0) {
            notes.push_back(rootMidi + tOffset);
        }
    }

    // ボイシング適用
    if (voicing == VoicingStyle::Drop2 && notes.size() >= 4) {
        // 上から2番目の音を1オクターブ下げる
        int dropIdx = static_cast<int>(notes.size()) - 2;
        notes[dropIdx] -= 12;
        std::sort(notes.begin(), notes.end());
    } else if (voicing == VoicingStyle::Drop3 && notes.size() >= 4) {
        // 上から3番目の音を1オクターブ下げる
        int dropIdx = static_cast<int>(notes.size()) - 3;
        notes[dropIdx] -= 12;
        std::sort(notes.begin(), notes.end());
    } else if (voicing == VoicingStyle::OpenSpread && notes.size() >= 3) {
        // 奇数番目の音を1オクターブ上げる
        for (size_t i = 1; i < notes.size(); i += 2) {
            notes[i] += 12;
        }
        std::sort(notes.begin(), notes.end());
    }

    // オンベース（分数コード）がある場合、低音域にベース音を追加
    if (chord.bassPitch != PitchClass::Unknown) {
        int bassMidi = 12 * (baseOctave - 1) + static_cast<int>(chord.bassPitch);
        notes.insert(notes.begin(), bassMidi);
    } else {
        // ルート音を1オクターブ下のベース音として補強
        int bassMidi = rootMidi - 12;
        notes.insert(notes.begin(), bassMidi);
    }

    return notes;
}

std::vector<std::string> MusicEngine::GetCircleOfFifths(bool counterClockwise) {
    static const std::vector<std::string> kCircleFifths = {
        "C", "G", "D", "A", "E", "B", "F#", "Db", "Ab", "Eb", "Bb", "F"
    };
    if (!counterClockwise) return kCircleFifths;
    std::vector<std::string> rev(kCircleFifths.rbegin(), kCircleFifths.rend());
    return rev;
}

} // namespace clp
