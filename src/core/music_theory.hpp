#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <string_view>
#include <optional>
#include <algorithm>
#include <array>

namespace clp {

// Tick 時間管理定数 (Standard MIDI 480 TPQN 準拠)
constexpr int TICKS_PER_BEAT = 480;       // 1拍 (4分音符) = 480 ticks
constexpr int TICKS_PER_BAR_4_4 = 1920;   // 4/4拍子の1小節 = 1920 ticks

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
    int durationBeats = 1;     // 音価（拍数：互換用）
    int durationTicks = TICKS_PER_BEAT; // 音価（Tick時間：480 ticks = 1拍。一次情報）
    bool isRest = false;       // 休符フラグ
    int inversion = 0;         // 転回形 (0: 基本形, 1: 第1転回, 2: 第2転回, 3: 第3転回)
    int octaveOffset = 0;      // コード単位のオクターブシフト (-3〜+3)
    // 構成音ごとのオクターブ移動（ピアノロールのドラッグ）。添字は音名 (0=C〜11=B)、値は -3〜+3 オクターブ
    std::array<int8_t, 12> toneShift{};

    bool HasToneShift() const {
        for (int8_t s : toneShift) { if (s != 0) return true; }
        return false;
    }

    // tick から拍数を更新するヘルパー
    void SetTicks(int ticks) {
        durationTicks = std::max(1, ticks);
        durationBeats = std::max(1, (durationTicks + TICKS_PER_BEAT / 2) / TICKS_PER_BEAT);
    }

    // 拍数から tick を更新するヘルパー
    void SetBeats(int beats) {
        durationBeats = std::max(1, beats);
        durationTicks = durationBeats * TICKS_PER_BEAT;
    }

    bool operator==(const ChordToken& other) const = default;
};

struct Bar {
    int barNumber = 1;
    int timeNumerator = 4;
    int timeDenominator = 4;
    std::vector<ChordToken> chords; // 小節内のコード列
    std::string keyChange;          // 転調マーカー（空文字でなければこの小節から転調）
    // 旧形式のマーカー（テキスト記法 [名前] の読込と旧ファイルの変換用）。
    // 現行のマーカーは Part::markers（自由位置）で持つ。MIDI 出力は両方を扱う。
    std::string sectionMarker;

    // 小節全体の総 tick 数を取得
    int GetTotalTicks() const {
        // 拍子分子 * (TICKS_PER_BEAT * 4 / 拍子分母)
        int beatTicks = (timeDenominator > 0) ? (TICKS_PER_BEAT * 4 / timeDenominator) : TICKS_PER_BEAT;
        return timeNumerator * beatTicks;
    }
};

// 自由位置マーカー。tick はパート先頭（マージ後は曲頭）からの絶対位置。
struct PartMarker {
    int tick = 0;
    std::string name;

    bool operator==(const PartMarker& other) const = default;
};

struct Part {
    std::string id;           // 一意のID (UUIDまたは短縮文字列)
    std::string name;          // 表示名 (例: "Intro", "Aメロ", "サビ")
    std::string keyRoot = "C"; // パート固有のキー
    MusicalMode mode = MusicalMode::Major;
    int bpm = 120;
    int timeNumerator = 4;
    int timeDenominator = 4;
    std::vector<Bar> bars;
    std::string rawProgressionText; // パートのコードテキスト
    std::vector<PartMarker> markers; // 自由位置マーカー（tick 昇順・同一 tick は1件）
};

} // namespace clp
