#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <string_view>
#include <optional>

namespace clp {

enum class PitchClass : uint8_t {
    C = 0, Cs = 1, D = 2, Ds = 3, E = 4, F = 5,
    Fs = 6, G = 7, Gs = 8, A = 9, As = 10, B = 11,
    Unknown = 255
};

enum class AccidentalPreference {
    Auto,
    Sharp,
    Flat
};

enum class ChordQuality : uint8_t {
    Major,
    Minor,
    Dominant7,
    Major7,
    Minor7,
    MinorMajor7,
    Diminished,
    Diminished7,
    HalfDiminished7, // m7b5
    Augmented,
    Augmented7,
    Sus2,
    Sus4,
    Dominant7Sus4,
    Add9,
    MinorAdd9,
    Six,
    MinorSix,
    Power5,
    Rest, // 休符 (N.C.)
    Custom
};

enum class MusicalMode : uint8_t {
    Major = 0,      // Ionian
    NaturalMinor,   // Aeolian
    HarmonicMinor,
    MelodicMinor,
    Dorian,
    Phrygian,
    Lydian,
    Mixolydian,
    Locrian
};

enum class VoicingStyle : uint8_t {
    RootPosition = 0,
    Drop2,
    Drop3,
    OpenSpread,
    Closed
};

struct ChordToken {
    std::string rawText;       // 元の入力文字列
    std::string rootName;      // 例: "C", "F#"
    PitchClass rootPitch = PitchClass::Unknown;
    ChordQuality quality = ChordQuality::Major;
    std::string qualitySuffix; // 例: "maj7", "m7b5"
    std::vector<int8_t> tensions; // 例: 9, 11, 13, -9 (b9), +9 (#9), -5 (b5)
    std::string bassName;      // オンコード / 分数コードのベース音
    PitchClass bassPitch = PitchClass::Unknown;
    std::string degreeRoman;   // 例: "I", "bIIImaj7", "V7/VI"
    int durationBeats = 1;     // 音価（拍数）
    bool isRest = false;       // 休符フラグ

    bool operator==(const ChordToken& other) const = default;
};

struct Bar {
    int barNumber = 1;
    int timeNumerator = 4;
    int timeDenominator = 4;
    std::vector<ChordToken> chords; // 小節内のコード列（拍数合計が拍子と一致）
    std::string keyChange;          // 転調マーカー（空文字でなければこの小節から転調）
    std::string sectionMarker;      // 例: "Intro", "A", "Chorus"
};

struct Part {
    std::string id;            // 一意のID (UUIDまたは短縮文字列)
    std::string name;          // 表示名 (例: "Intro", "Aメロ", "サビ")
    std::string keyRoot = "C"; // パート固有のキー
    MusicalMode mode = MusicalMode::Major;
    int bpm = 120;
    int timeNumerator = 4;
    int timeDenominator = 4;
    std::vector<Bar> bars;
    std::string rawProgressionText; // パートのコードテキスト
};

} // namespace clp
